/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cell::RefCell;
use std::ffi::OsString;
use std::fs::{create_dir_all, read_dir, read_to_string, File};
use std::io::{Error, ErrorKind};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::Arc;

use bincode;
use fxhash;
use nsstring::nsAString;
use rayon::ThreadPool;
use webrender::{ProgramBinary, ProgramCache, ProgramCacheObserver, ProgramSourceDigest};

const MAX_LOAD_TIME_MS: u64 = 400;

fn deserialize_program_binary(path: &Path) -> Result<Arc<ProgramBinary>, Error> {
    let mut buf = vec![];
    let mut file = File::open(path)?;
    file.read_to_end(&mut buf)?;

    if buf.len() <= 8 + 4 {
        return Err(Error::new(ErrorKind::InvalidData, "File size is too small"));
    }
    let magic = &buf[0..4];
    let hash = &buf[4..8 + 4];
    let data = &buf[8 + 4..];

    // Check if magic + version are correct.
    let mv: u32 = bincode::deserialize(&magic).unwrap();
    if mv != MAGIC_AND_VERSION {
        return Err(Error::new(
            ErrorKind::InvalidData,
            "File data is invalid (magic+version)",
        ));
    }

    // Check if hash is correct
    let hash: u64 = bincode::deserialize(&hash).unwrap();
    let hash_data = fxhash::hash64(&data);
    if hash != hash_data {
        return Err(Error::new(ErrorKind::InvalidData, "File data is invalid (hash)"));
    }

    // Deserialize ProgramBinary
    let binary = match bincode::deserialize(&data) {
        Ok(binary) => binary,
        Err(_) => {
            return Err(Error::new(
                ErrorKind::InvalidData,
                "Failed to deserialize ProgramBinary",
            ))
        }
    };

    Ok(Arc::new(binary))
}

#[cfg(target_os = "windows")]
fn get_cache_path_from_prof_path(prof_path: &nsAString) -> Option<PathBuf> {
    if prof_path.is_empty() {
        // Empty means that we do not use disk cache.
        return None;
    }

    use std::os::windows::prelude::*;

    let prof_path = OsString::from_wide(prof_path.as_ref());
    let mut cache_path = PathBuf::from(&prof_path);
    cache_path.push("shader-cache");

    Some(cache_path)
}

#[cfg(not(target_os = "windows"))]
fn get_cache_path_from_prof_path(prof_path: &nsAString) -> Option<PathBuf> {
    if prof_path.is_empty() {
        // Empty means that we do not use disk cache.
        return None;
    }

    let utf8 = String::from_utf16(prof_path.as_ref()).unwrap();
    let prof_path = OsString::from(utf8);
    let mut cache_path = PathBuf::from(&prof_path);
    cache_path.push("shader-cache");

    Some(cache_path)
}

struct WrProgramBinaryDiskCache {
    cache_path: PathBuf,
    workers: Arc<ThreadPool>,
    cached_shader_filenames: Vec<OsString>,
}

// Magic number + version. Increment the version when the binary format changes.
const MAGIC: u32 = 0xB154AD30; // BI-SHADE + version.
const VERSION: u32 = 2;
const MAGIC_AND_VERSION: u32 = MAGIC + VERSION;

const WHITELIST_FILENAME: &str = "startup_shaders";
const WHITELIST_SEPARATOR: &str = "\n";

/// Helper to convert a closure returning a `Result` to one that returns void.
/// This allows the enclosed code to use the question-mark operator in a
/// context where the calling function doesn't expect a `Result`.
#[allow(unused_must_use)]
fn result_to_void<F: FnOnce() -> Result<(), ()>>(f: F) {
    f();
}

impl WrProgramBinaryDiskCache {
    #[allow(dead_code)]
    fn new(cache_path: PathBuf, workers: &Arc<ThreadPool>) -> Self {
        WrProgramBinaryDiskCache {
            cache_path,
            workers: Arc::clone(workers),
            cached_shader_filenames: Vec::new(),
        }
    }

    /// Saves the specified binaries to the on-disk shader cache.
    fn save_shaders_to_disk(&mut self, entries: Vec<Arc<ProgramBinary>>) {
        info!("Saving binaries to on-disk shader cache");

        // Write the entries to disk on a worker thread.
        for entry in entries {
            let file_name = entry.source_digest().to_string();
            let file_path = self.cache_path.join(&file_name);

            self.workers.spawn(move || {
                result_to_void(move || {
                    info!("Writing shader: {}", file_name);

                    use std::time::Instant;
                    let start = Instant::now();

                    let data: Vec<u8> =
                        bincode::serialize(&*entry).map_err(|e| error!("shader-cache: Failed to serialize: {}", e))?;

                    let mut file =
                        File::create(&file_path).map_err(|e| error!("shader-cache: Failed to create file: {}", e))?;

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
                })
            });
        }
    }

    /// Writes the whitelist containing the set of startup shaders to disk.
    fn set_startup_shaders(&mut self, entries: Vec<Arc<ProgramBinary>>) {
        let whitelist = entries
            .iter()
            .map(|e| e.source_digest().to_string())
            .collect::<Vec<String>>()
            .join(WHITELIST_SEPARATOR);

        let mut whitelist_path = self.cache_path.clone();
        whitelist_path.push(WHITELIST_FILENAME);
        self.workers.spawn(move || {
            result_to_void(move || {
                info!("Writing startup shader whitelist");
                File::create(&whitelist_path)
                    .and_then(|mut file| file.write_all(whitelist.as_bytes()))
                    .map_err(|e| error!("shader-cache: Failed to write startup whitelist: {}", e))?;
                Ok(())
            })
        });
    }

    pub fn try_load_shader_from_disk(&mut self, filename: &str, program_cache: &Rc<ProgramCache>) {
        if let Some(index) = self.cached_shader_filenames.iter().position(|e| e == filename) {
            let mut path = self.cache_path.clone();
            path.push(filename);

            self.cached_shader_filenames.swap_remove(index);

            info!("Loading shader: {}", filename);

            match deserialize_program_binary(&path) {
                Ok(program) => {
                    program_cache.load_program_binary(program);
                }
                Err(err) => {
                    error!("shader-cache: Failed to deserialize program binary: {}", err);
                }
            };
        } else {
            info!("shader-cache: Program binary not found in disk cache");
        }
    }

    pub fn try_load_startup_shaders_from_disk(&mut self, program_cache: &Rc<ProgramCache>) {
        use std::time::Instant;
        let start = Instant::now();

        // Load and parse the whitelist if it exists
        let mut whitelist_path = self.cache_path.clone();
        whitelist_path.push(WHITELIST_FILENAME);
        let whitelist = match read_to_string(&whitelist_path) {
            Ok(whitelist) => whitelist
                .split(WHITELIST_SEPARATOR)
                .map(|s| s.to_string())
                .collect::<Vec<String>>(),
            Err(err) => {
                info!("shader-cache: Could not read startup whitelist: {}", err);
                Vec::new()
            }
        };
        info!("Loaded startup shader whitelist in {:?}", start.elapsed());

        self.cached_shader_filenames = read_dir(&self.cache_path)
            .map_err(|err| {
                error!(
                    "shader-cache: Error reading directory whilst loading startup shaders: {}",
                    err
                )
            })
            .into_iter()
            .flatten()
            .filter_map(Result::ok)
            .map(|entry| entry.file_name())
            .filter(|filename| filename != WHITELIST_FILENAME)
            .collect::<Vec<OsString>>();

        // Load whitelisted program binaries if they exist
        for entry in &whitelist {
            self.try_load_shader_from_disk(&entry, program_cache);

            let elapsed = start.elapsed();
            info!("Loaded shader in {:?}", elapsed);
            let elapsed_ms = (elapsed.as_secs() * 1_000) + elapsed.subsec_millis() as u64;

            if elapsed_ms > MAX_LOAD_TIME_MS {
                // Loading the startup shaders is taking too long, so bail out now.
                // Additionally clear the list of remaining shaders cached on disk,
                // so that we do not attempt to load any on demand during rendering.
                error!("shader-cache: Timed out before finishing loads");
                self.cached_shader_filenames.clear();
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
        WrProgramCacheObserver { disk_cache }
    }
}

impl ProgramCacheObserver for WrProgramCacheObserver {
    fn save_shaders_to_disk(&self, entries: Vec<Arc<ProgramBinary>>) {
        self.disk_cache.borrow_mut().save_shaders_to_disk(entries);
    }

    fn set_startup_shaders(&self, entries: Vec<Arc<ProgramBinary>>) {
        self.disk_cache.borrow_mut().set_startup_shaders(entries);
    }

    fn try_load_shader_from_disk(&self, digest: &ProgramSourceDigest, program_cache: &Rc<ProgramCache>) {
        let filename = digest.to_string();
        self.disk_cache
            .borrow_mut()
            .try_load_shader_from_disk(&filename, program_cache);
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
            let cache = Rc::new(RefCell::new(WrProgramBinaryDiskCache::new(
                cache_path.unwrap(),
                workers,
            )));
            let obs = Box::new(WrProgramCacheObserver::new(Rc::clone(&cache))) as Box<dyn ProgramCacheObserver>;
            (Some(cache), Some(obs))
        } else {
            (None, None)
        };
        let program_cache = ProgramCache::new(program_cache_observer);

        WrProgramCache {
            program_cache,
            disk_cache,
        }
    }

    pub fn rc_get(&self) -> &Rc<ProgramCache> {
        &self.program_cache
    }

    pub fn try_load_startup_shaders_from_disk(&self) {
        if let Some(ref disk_cache) = self.disk_cache {
            disk_cache
                .borrow_mut()
                .try_load_startup_shaders_from_disk(&self.program_cache);
        } else {
            error!("shader-cache: Shader disk cache is not supported");
        }
    }
}

pub fn remove_disk_cache(prof_path: &nsAString) -> Result<(), Error> {
    use std::fs::remove_dir_all;
    use std::time::Instant;

    if let Some(cache_path) = get_cache_path_from_prof_path(prof_path) {
        if cache_path.exists() {
            let start = Instant::now();
            remove_dir_all(&cache_path)?;
            info!("removed all disk cache shaders in {:?}", start.elapsed());
        }
    }
    Ok(())
}
