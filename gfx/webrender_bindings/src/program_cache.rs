use std::cell::RefCell;
use std::io::{Error, ErrorKind};
use std::fs::{File, create_dir_all, read_dir};
use std::io::{Read, Write};
use std::path::{PathBuf};
use std::rc::Rc;
use std::sync::Arc;

use webrender::{ProgramBinary, ProgramCache, ProgramCacheObserver};
use bincode;
use fxhash;
use nsstring::nsAString;
use rayon::ThreadPool;

const MAX_LOAD_TIME_MS: u64 = 400;

fn deserialize_program_binary(path: &PathBuf) -> Result<Arc<ProgramBinary>, Error> {
    let mut buf = vec![];
    let mut file = File::open(path)?;
    file.read_to_end(&mut buf)?;

    if buf.len() <= 8 + 4 {
        return Err(Error::new(ErrorKind::InvalidData, "File size is too small"));
    }
    let magic = &buf[0 .. 4];
    let hash = &buf[4 .. 8 + 4];
    let data = &buf[8 + 4 ..];

    // Check if magic + version are correct.
    let mv:u32 = bincode::deserialize(&magic).unwrap();
    if mv != MAGIC_AND_VERSION {
        return Err(Error::new(ErrorKind::InvalidData, "File data is invalid (magic+version)"));
    }

    // Check if hash is correct
    let hash:u64 = bincode::deserialize(&hash).unwrap();
    let hash_data = fxhash::hash64(&data);
    if hash != hash_data {
        return Err(Error::new(ErrorKind::InvalidData, "File data is invalid (hash)"));
    }

    // Deserialize ProgramBinary
    let binary = match bincode::deserialize(&data) {
        Ok(binary) => binary,
        Err(_) => return Err(Error::new(ErrorKind::InvalidData, "Failed to deserialize ProgramBinary")),
    };

    Ok(Arc::new(binary))
}

#[cfg(target_os = "windows")]
fn get_cache_path_from_prof_path(prof_path: &nsAString) -> Option<PathBuf> {
    if prof_path.is_empty() {
        // Empty means that we do not use disk cache.
        return None;
    }

    use std::ffi::OsString;
    use std::os::windows::prelude::*;

    let prof_path = OsString::from_wide(prof_path.as_ref());
    let mut cache_path = PathBuf::from(&prof_path);
    cache_path.push("shader-cache");

    Some(cache_path)
}

#[cfg(not(target_os="windows"))]
fn get_cache_path_from_prof_path(prof_path: &nsAString) -> Option<PathBuf> {
    if prof_path.is_empty() {
        // Empty means that we do not use disk cache.
        return None;
    }

    use std::ffi::OsString;

    let utf8 = String::from_utf16(prof_path.as_ref()).unwrap();
    let prof_path = OsString::from(utf8);
    let mut cache_path = PathBuf::from(&prof_path);
    cache_path.push("shader-cache");

    Some(cache_path)
}

struct WrProgramBinaryDiskCache {
    cache_path: PathBuf,
    workers: Arc<ThreadPool>,
}

// Magic number + version. Increment the version when the binary format changes.
const MAGIC: u32 = 0xB154AD30; // BI-SHADE + version.
const VERSION: u32 = 2;
const MAGIC_AND_VERSION: u32 = MAGIC + VERSION;

/// Helper to convert a closure returning a `Result` to one that returns void.
/// This allows the enclosed code to use the question-mark operator in a
/// context where the calling function doesn't expect a `Result`.
#[allow(unused_must_use)]
fn result_to_void<F: FnOnce() -> Result<(), ()>>(f: F) { f(); }

impl WrProgramBinaryDiskCache {
    #[allow(dead_code)]
    fn new(cache_path: PathBuf, workers: &Arc<ThreadPool>) -> Self {
        WrProgramBinaryDiskCache {
            cache_path,
            workers: Arc::clone(workers),
        }
    }

    /// Updates the on-disk cache to contain exactly the entries specified.
    fn update(&mut self, entries: Vec<Arc<ProgramBinary>>) {
        info!("Updating on-disk shader cache");

        // Compute the digests in string form.
        let mut entries: Vec<(String, Arc<ProgramBinary>)> =
            entries.into_iter().map(|e| (format!("{}", e.source_digest()), e)).collect();

        // For each file in the current directory, check if it corresponds to
        // an entry we're supposed to write. If so, we don't need to write the
        // entry. If not, we delete the file.
        for existing in read_dir(&self.cache_path).unwrap().filter_map(|f| f.ok()) {
            let pos = existing.file_name().to_str()
                .and_then(|digest| entries.iter().position(|x| x.0 == digest));
            if let Some(p) = pos {
                info!("Found existing shader: {}", existing.file_name().to_string_lossy());
                entries.swap_remove(p);
            } else {
                self.workers.spawn(move || {
                    info!("Removing shader: {}", existing.file_name().to_string_lossy());
                    ::std::fs::remove_file(existing.path())
                        .unwrap_or_else(|e| error!("shader-cache: Failed to remove shader: {:?}", e));
                });
            }
        }

        // Write the remaining entries to disk on a worker thread.
        for entry in entries.into_iter() {
            let (file_name, program_binary) = entry;
            let file_path = self.cache_path.join(&file_name);

            self.workers.spawn(move || result_to_void(move || {
                info!("Writing shader: {}", file_name);

                use std::time::{Instant};
                let start = Instant::now();

                let data: Vec<u8> = bincode::serialize(&*program_binary)
                    .map_err(|e| error!("shader-cache: Failed to serialize: {}", e))?;

                let mut file = File::create(&file_path)
                    .map_err(|e| error!("shader-cache: Failed to create file: {}", e))?;

                // Write magic + version.
                let mv = MAGIC_AND_VERSION;
                let mv = bincode::serialize(&mv).unwrap();
                assert!(mv.len() == 4);
                file.write_all(&mv)
                    .map_err(|e| error!("shader-cache: Failed to write magic + version: {}", e))?;

                // Write hash
                let hash = fxhash::hash64(&data);
                let hash = bincode::serialize(&hash).unwrap();
                assert!(hash.len() == 8);
                file.write_all(&hash)
                    .map_err(|e| error!("shader-cache: Failed to write hash: {}", e))?;

                // Write serialized data
                file.write_all(&data)
                    .map_err(|e| error!("shader-cache: Failed to write program binary: {}", e))?;

                info!("Wrote shader {} in {:?}", file_name, start.elapsed());
                Ok(())
            }));
        }
    }

    pub fn try_load_from_disk(&mut self, program_cache: &Rc<ProgramCache>) {
        use std::time::{Instant};
        let start = Instant::now();

        // Load program binaries if exist
        for entry in read_dir(&self.cache_path).unwrap() {
            let entry = entry.unwrap();
            let path = entry.path();

            info!("Loading shader: {}", entry.file_name().to_string_lossy());

            match deserialize_program_binary(&path) {
                Ok(program) => {
                    program_cache.load_program_binary(program);
                }
                Err(err) => {
                    error!("shader-cache: Failed to deserialize program binary: {}", err);
                }
            };

            let elapsed = start.elapsed();
            info!("Loaded shader in {:?}", elapsed);
            let elapsed_ms = (elapsed.as_secs() * 1_000) +
                (elapsed.subsec_nanos() / 1_000_000) as u64;

            if elapsed_ms > MAX_LOAD_TIME_MS {
                error!("shader-cache: Timed out before finishing loads");
                break;
            }
        }
    }
}

pub struct WrProgramCacheObserver {
    disk_cache: Rc<RefCell<WrProgramBinaryDiskCache>>,
}

impl WrProgramCacheObserver {
    #[allow(dead_code)]
    fn new(disk_cache: Rc<RefCell<WrProgramBinaryDiskCache>>) -> Self {
        WrProgramCacheObserver{
            disk_cache,
        }
    }
}

impl ProgramCacheObserver for WrProgramCacheObserver {
    fn update_disk_cache(&self, entries: Vec<Arc<ProgramBinary>>) {
        self.disk_cache.borrow_mut().update(entries);
    }

    fn notify_program_binary_failed(&self, _program_binary: &Arc<ProgramBinary>) {
        error!("shader-cache: Failed program_binary");
    }
}


pub struct WrProgramCache {
    pub program_cache: Rc<ProgramCache>,
    disk_cache: Option<Rc<RefCell<WrProgramBinaryDiskCache>>>,
}

impl WrProgramCache {
    pub fn new(prof_path: &nsAString, workers: &Arc<ThreadPool>) -> Self {
        let cache_path = get_cache_path_from_prof_path(prof_path);
        let use_disk_cache = cache_path.as_ref().map_or(false, |p| create_dir_all(p).is_ok());
        let (disk_cache, program_cache_observer) = if use_disk_cache {
            let cache = Rc::new(RefCell::new(WrProgramBinaryDiskCache::new(cache_path.unwrap(), workers)));
            let obs = Box::new(WrProgramCacheObserver::new(Rc::clone(&cache))) as
                Box<dyn ProgramCacheObserver>;
            (Some(cache), Some(obs))
        } else {
            (None, None)
        };
        let program_cache = ProgramCache::new(program_cache_observer);

        WrProgramCache {
            program_cache,
            disk_cache: disk_cache,
        }
    }

    pub fn rc_get(&self) -> &Rc<ProgramCache> {
        &self.program_cache
    }

    pub fn try_load_from_disk(&self) {
        if let Some(ref disk_cache) = self.disk_cache {
            disk_cache.borrow_mut().try_load_from_disk(&self.program_cache);
        } else {
            error!("shader-cache: Shader disk cache is not supported");
        }
    }
}

pub fn remove_disk_cache(prof_path: &nsAString) -> Result<(), Error> {
    use std::fs::remove_dir_all;
    use std::time::{Instant};

    if let Some(cache_path) = get_cache_path_from_prof_path(prof_path) {
        if cache_path.exists() {
            let start = Instant::now();
            remove_dir_all(&cache_path)?;
            info!("removed all disk cache shaders in {:?}", start.elapsed());
        }
    }
    Ok(())
}
