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
use uuid::Uuid;

const MAX_LOAD_TIME_MS: u64 = 400;
const MAX_CACHED_PROGRAM_COUNT: u32 = 15;

fn deserialize_program_binary(path: &PathBuf) -> Result<Arc<ProgramBinary>, Error> {
    let mut buf = vec![];
    let mut file = File::open(path)?;
    file.read_to_end(&mut buf)?;

    if buf.len() <= 8 {
        return Err(Error::new(ErrorKind::InvalidData, "File size is too small"));
    }
    let hash = &buf[0 .. 8];
    let data = &buf[8 ..];

    // Check if hash is correct
    let hash:u64 = bincode::deserialize(&hash).unwrap();
    let hash_data = fxhash::hash64(&data);
    if hash != hash_data {
        return Err(Error::new(ErrorKind::InvalidData, "File data is invalid"));
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
fn get_cache_path_from_prof_path(_prof_path: &nsAString) -> Option<PathBuf> {
    // Not supported yet.
    None
}

struct WrProgramBinaryDiskCache {
    cache_path: Option<PathBuf>,
    program_count: u32,
    is_enabled: bool,
    workers: Arc<ThreadPool>,
}

impl WrProgramBinaryDiskCache {
    #[allow(dead_code)]
    fn new(prof_path: &nsAString, workers: &Arc<ThreadPool>) -> Self {
        let cache_path = get_cache_path_from_prof_path(prof_path);
        let is_enabled = cache_path.is_some();
        let workers = Arc::clone(workers);

        WrProgramBinaryDiskCache{
            cache_path,
            program_count: 0,
            is_enabled,
            workers,
        }
    }

    fn notify_binary_added(&mut self, program_binary: &Arc<ProgramBinary>) {
        if !self.is_enabled {
            return;
        }

        if let Some(ref cache_path) = self.cache_path {
            if let Err(_) = create_dir_all(&cache_path) {
                error!("failed to create dir for shader disk cache");
                return;
            }

            self.program_count += 1;
            if self.program_count > MAX_CACHED_PROGRAM_COUNT {
                // Disable disk cache to avoid storing more shader programs to disk
                self.is_enabled = false;
                return;
            }

            // Use uuid for file name
            let uuid1 = Uuid::new_v4();
            let file_name = uuid1.to_hyphenated_string();
            let program_binary = Arc::clone(program_binary);
            let file_path = cache_path.join(&file_name);

            let program_count = self.program_count;

            // Save to disk on worker thread
            self.workers.spawn(move || {

                use std::time::{Instant};
                let start = Instant::now();

                let data: Vec<u8> = match bincode::serialize(&*program_binary) {
                    Ok(data) => data,
                    Err(err) => {
                        error!("Failed to serialize program binary error: {}", err);
                        return;
                    }
                };

                let mut file = match File::create(&file_path) {
                    Ok(file) => file,
                    Err(err) => {
                        error!("Unable to create file for program binary error: {}", err);
                        return;
                    }
                };

                // Write hash
                let hash = fxhash::hash64(&data);
                let hash = bincode::serialize(&hash).unwrap();
                assert!(hash.len() == 8);
                match file.write_all(&hash) {
                    Err(err) => {
                        error!("Failed to write hash to file error: {}", err);
                    }
                    _ => {},
                };

                // Write serialized data
                match file.write_all(&data) {
                    Err(err) => {
                        error!("Failed to write program binary to file error: {}", err);
                    }
                    _ => {},
                };

                let elapsed = start.elapsed();
                info!("notify_binary_added: {} ms program_count {}",
                    (elapsed.as_secs() * 1_000) + (elapsed.subsec_nanos() / 1_000_000) as u64, program_count);

            });
        }
    }

    pub fn try_load_from_disk(&mut self, program_cache: &Rc<ProgramCache>) {
        if !self.is_enabled {
            return;
        }

        if let Some(ref cache_path) = self.cache_path {
            use std::time::{Instant};
            let start = Instant::now();

            // Load program binaries if exist
            if cache_path.exists() && cache_path.is_dir() {
                for entry in read_dir(cache_path).unwrap() {
                    let entry = entry.unwrap();
                    let path = entry.path();

                    info!("loading shader file");

                    match deserialize_program_binary(&path) {
                        Ok(program) => {
                            program_cache.load_program_binary(program);
                        }
                        Err(err) => {
                            error!("Failed to desriralize program binary error: {}", err);
                        }
                    };

                    self.program_count += 1;

                    let elapsed = start.elapsed();
                    let elapsed_ms = (elapsed.as_secs() * 1_000) + (elapsed.subsec_nanos() / 1_000_000) as u64;
                    info!("deserialize_program_binary: {} ms program_count {}", elapsed_ms, self.program_count);

                    if self.program_count > MAX_CACHED_PROGRAM_COUNT || elapsed_ms > MAX_LOAD_TIME_MS {
                        // Disable disk cache to avoid storing more shader programs to disk
                        self.is_enabled = false;
                        break;
                    }
                }
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
    fn notify_binary_added(&self, program_binary: &Arc<ProgramBinary>) {
        self.disk_cache.borrow_mut().notify_binary_added(program_binary);
    }

    fn notify_program_binary_failed(&self, _program_binary: &Arc<ProgramBinary>) {
        error!("Failed program_binary");
    }
}


pub struct WrProgramCache {
    program_cache: Rc<ProgramCache>,
    disk_cache: Option<Rc<RefCell<WrProgramBinaryDiskCache>>>,
}

impl WrProgramCache {
    #[cfg(target_os = "windows")]
    pub fn new(prof_path: &nsAString, workers: &Arc<ThreadPool>) -> Self {
        let disk_cache = Rc::new(RefCell::new(WrProgramBinaryDiskCache::new(prof_path, workers)));
        let program_cache_observer = Box::new(WrProgramCacheObserver::new(Rc::clone(&disk_cache)));
        let program_cache = ProgramCache::new(Some(program_cache_observer));

        WrProgramCache {
            program_cache,
            disk_cache: Some(disk_cache),
        }
    }

    #[cfg(not(target_os="windows"))]
    pub fn new(_prof_path: &nsAString, _: &Arc<ThreadPool>) -> Self {
        let program_cache = ProgramCache::new(None);

        WrProgramCache {
            program_cache,
            disk_cache: None,
        }
    }

    pub fn rc_get(&self) -> &Rc<ProgramCache> {
        &self.program_cache
    }

    pub fn try_load_from_disk(&self) {
        if let Some(ref disk_cache) = self.disk_cache {
            disk_cache.borrow_mut().try_load_from_disk(&self.program_cache);
        } else {
            error!("Shader disk cache is not supported");
        }
    }
}

#[cfg(target_os = "windows")]
pub fn remove_disk_cache(prof_path: &nsAString) -> Result<(), Error> {
    use std::fs::remove_dir_all;
    use std::time::{Instant};

    if let Some(cache_path) = get_cache_path_from_prof_path(prof_path) {
        if cache_path.exists() {
            let start = Instant::now();

            remove_dir_all(&cache_path)?;

            let elapsed = start.elapsed();
            let elapsed_ms = (elapsed.as_secs() * 1_000) + (elapsed.subsec_nanos() / 1_000_000) as u64;
            info!("remove_disk_cache: {} ms", elapsed_ms);
        }
    }
    Ok(())
}

#[cfg(not(target_os="windows"))]
pub fn remove_disk_cache(_prof_path: &nsAString) -> Result<(), Error> {
    error!("Shader disk cache is not supported");
    return Err(Error::new(ErrorKind::Other, "Not supported"))
}

