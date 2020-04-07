/* Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! CPU detection and configuration of Cranelift's `TargetISA`.
//!
//! This module deals with the configuration of Cranelift to generate code for the current CPU that
//! is compatible with the SpiderMonkey JIT.
//!
//! The main entry point is the `make_isa()` function which allocates a configured `TargetISA`
//! object.

use std::env;

use cranelift_codegen::isa;
use cranelift_codegen::settings::{self, Configurable};

use crate::bindings::StaticEnvironment;
use crate::utils::{BasicError, DashResult};

#[cfg(target_pointer_width = "64")]
pub const POINTER_SIZE: usize = 8;
#[cfg(target_pointer_width = "32")]
pub const POINTER_SIZE: usize = 4;

impl From<isa::LookupError> for BasicError {
    fn from(err: isa::LookupError) -> BasicError {
        BasicError::new(err.to_string())
    }
}

impl From<settings::SetError> for BasicError {
    fn from(err: settings::SetError) -> BasicError {
        BasicError::new(err.to_string())
    }
}

struct EnvVariableFlags<'env> {
    opt_level: Option<&'env str>,
    jump_tables: Option<bool>,
}

#[inline]
fn str_to_bool(value: &str) -> bool {
    value == "true" || value == "on" || value == "yes" || value == "1"
}

impl<'env> EnvVariableFlags<'env> {
    fn parse(input: &'env Result<String, env::VarError>) -> Option<Self> {
        let input = match input {
            Ok(input) => input.as_str(),
            Err(_) => return None,
        };

        let mut flags = EnvVariableFlags {
            opt_level: None,
            jump_tables: None,
        };

        for entry in input.split(",") {
            if let Some(equals_index) = entry.find("=") {
                let (key, value) = entry.split_at(equals_index);

                // value starts with the =, remove it.
                let value = &value[1..];

                match key {
                    "opt_level" => {
                        // Invalid values will be reported by Cranelift.
                        flags.opt_level = Some(value);
                    }
                    "jump_tables" => {
                        flags.jump_tables = Some(str_to_bool(value));
                    }
                    _ => {
                        warn!("Unknown setting with key {}", key);
                    }
                }
            } else {
                warn!("Missing = in pair: {}", entry);
            }
        }

        Some(flags)
    }
}

/// Create a `Flags` object for the shared settings.
///
/// This only fails if one of Cranelift's settings has been removed or renamed.
fn make_shared_flags(
    env: &StaticEnvironment,
    env_flags: &Option<EnvVariableFlags>,
) -> settings::SetResult<settings::Flags> {
    let mut sb = settings::builder();

    // We don't install SIGFPE handlers, but depend on explicit traps around divisions.
    sb.enable("avoid_div_traps")?;

    // Cranelift needs to know how many words are pushed by `GenerateFunctionPrologue` so it can
    // compute frame pointer offsets accurately. C++'s "sizeof" gives us the number of bytes, which
    // we translate to the number of words, as expected by Cranelift.
    debug_assert_eq!(env.size_of_wasm_frame % POINTER_SIZE, 0);
    let num_words = env.size_of_wasm_frame / POINTER_SIZE;
    sb.set("baldrdash_prologue_words", &num_words.to_string())?;

    // Make sure that libcalls use the supplementary VMContext argument.
    let libcall_call_conv = if env.platform_is_windows {
        "baldrdash_windows"
    } else {
        "baldrdash_system_v"
    };
    sb.set("libcall_call_conv", libcall_call_conv)?;

    // Assembler::PatchDataWithValueCheck expects -1 stored where a function address should be
    // patched in.
    sb.enable("emit_all_ones_funcaddrs")?;

    // Enable the verifier if assertions are enabled. Otherwise leave it disabled,
    // as it's quite slow.
    if !cfg!(debug_assertions) {
        sb.set("enable_verifier", "false")?;
    }

    // Baldrdash does its own stack overflow checks, so we don't need Cranelift doing any for us.
    sb.set("enable_probestack", "false")?;

    // Let's optimize for speed by default.
    let opt_level = match env_flags {
        Some(env_flags) => env_flags.opt_level,
        None => None,
    }
    .unwrap_or("speed");
    sb.set("opt_level", opt_level)?;

    // Enable jump tables by default.
    let enable_jump_tables = match env_flags {
        Some(env_flags) => env_flags.jump_tables,
        None => None,
    }
    .unwrap_or(true);
    sb.set(
        "enable_jump_tables",
        if enable_jump_tables { "true" } else { "false" },
    )?;

    if cfg!(feature = "cranelift_x86") && cfg!(target_pointer_width = "64") {
        sb.enable("enable_pinned_reg")?;
        sb.enable("use_pinned_reg_as_heap_base")?;
    }

    if env.ref_types_enabled {
        sb.enable("enable_safepoints")?;
    }

    Ok(settings::Flags::new(sb))
}

#[cfg(feature = "cranelift_x86")]
fn make_isa_specific(env: &StaticEnvironment) -> DashResult<isa::Builder> {
    let mut ib = isa::lookup_by_name("x86_64-unknown-unknown").map_err(BasicError::from)?;

    if !env.has_sse2 {
        return Err("SSE2 is mandatory for Baldrdash!".into());
    }

    if env.has_sse3 {
        ib.enable("has_sse3").map_err(BasicError::from)?;
    }
    if env.has_sse41 {
        ib.enable("has_sse41").map_err(BasicError::from)?;
    }
    if env.has_sse42 {
        ib.enable("has_sse42").map_err(BasicError::from)?;
    }
    if env.has_popcnt {
        ib.enable("has_popcnt").map_err(BasicError::from)?;
    }
    if env.has_avx {
        ib.enable("has_avx").map_err(BasicError::from)?;
    }
    if env.has_bmi1 {
        ib.enable("has_bmi1").map_err(BasicError::from)?;
    }
    if env.has_bmi2 {
        ib.enable("has_bmi2").map_err(BasicError::from)?;
    }
    if env.has_lzcnt {
        ib.enable("has_lzcnt").map_err(BasicError::from)?;
    }

    Ok(ib)
}

#[cfg(not(feature = "cranelift_x86"))]
fn make_isa_specific(_env: &StaticEnvironment) -> DashResult<isa::Builder> {
    Err("Platform not supported yet!".into())
}

/// Allocate a `TargetISA` object that can be used to generate code for the CPU we're running on.
pub fn make_isa(env: &StaticEnvironment) -> DashResult<Box<dyn isa::TargetIsa>> {
    // Parse flags defined by the environment variable.
    let env_flags_str = std::env::var("CRANELIFT_FLAGS");
    let env_flags = EnvVariableFlags::parse(&env_flags_str);

    // Start with the ISA-independent settings.
    let shared_flags = make_shared_flags(env, &env_flags).map_err(BasicError::from)?;
    let ib = make_isa_specific(env)?;
    Ok(ib.finish(shared_flags))
}
