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

use cranelift_codegen::isa;
use cranelift_codegen::settings::{self, Configurable};
use std::error;
use std::mem;
use std::str::FromStr;
use target_lexicon;
use utils::{BasicError, DashResult};

use baldrdash::StaticEnvironment;

impl From<isa::LookupError> for BasicError {
    fn from(err: isa::LookupError) -> BasicError {
        let msg = match err {
            isa::LookupError::SupportDisabled => "ISA support is disabled",
            isa::LookupError::Unsupported => "unsupported ISA",
        };
        BasicError::new(msg.to_string())
    }
}

impl From<settings::SetError> for BasicError {
    fn from(err: settings::SetError) -> BasicError {
        let msg = match err {
            settings::SetError::BadName => "bad setting name",
            settings::SetError::BadType => "bad setting type",
            settings::SetError::BadValue => "bad setting value",
        };
        BasicError::new(msg.to_string())
    }
}

/// Allocate a `TargetISA` object that can be used to generate code for the CPU we're running on.
///
/// TODO: SM runs on more than x86 chips. Support them.
///
/// # Errors
///
/// This function fails if Cranelift has not been compiled with support for the current CPU.
pub fn make_isa(env: &StaticEnvironment) -> DashResult<Box<isa::TargetIsa>> {
    // Start with the ISA-independent settings.
    let shared_flags = make_shared_flags().expect("Cranelift configuration error");

    // We can use `#[cfg(target_arch = "...")]` to conditionally compile code per ISA.
    let mut ib = isa::lookup(triple!("x86_64-unknown-unknown")).map_err(BasicError::from)?;

    if !env.hasSse2 {
        return Err("SSE2 is mandatory for Baldrdash!".into());
    }
    if env.hasSse3 {
        ib.enable("has_sse3").map_err(BasicError::from)?;
    }
    if env.hasSse41 {
        ib.enable("has_sse41").map_err(BasicError::from)?;
    }
    if env.hasSse42 {
        ib.enable("has_sse42").map_err(BasicError::from)?;
    }
    if env.hasPopcnt {
        ib.enable("has_popcnt").map_err(BasicError::from)?;
    }
    if env.hasAvx {
        ib.enable("has_avx").map_err(BasicError::from)?;
    }
    if env.hasBmi1 {
        ib.enable("has_bmi1").map_err(BasicError::from)?;
    }
    if env.hasBmi2 {
        ib.enable("has_bmi2").map_err(BasicError::from)?;
    }
    if env.hasLzcnt {
        ib.enable("has_lzcnt").map_err(BasicError::from)?;
    }

    Ok(ib.finish(shared_flags))
}

/// Create a `Flags` object for the shared settings.
///
/// This only fails if one of Cranelift's settings has been removed or renamed.
fn make_shared_flags() -> settings::SetResult<settings::Flags> {
    let mut sb = settings::builder();

    // Since we're using SM's epilogue insertion code, we can only handle a single return
    // instruction at the end of the function.
    sb.enable("return_at_end")?;

    // We don't install SIGFPE handlers, but depend on explicit traps around divisions.
    sb.enable("avoid_div_traps")?;

    // Cranelift needs to know how many words are pushed by `GenerateFunctionPrologue` so it can
    // compute frame pointer offsets accurately.
    //
    // 1. Return address (whether explicitly pushed on ARM or implicitly on x86).
    // 2. TLS register.
    // 3. Previous frame pointer.
    //
    sb.set("baldrdash_prologue_words", "3")?;

    // Assembler::PatchDataWithValueCheck expects -1 stored where a function address should be
    // patched in.
    sb.enable("allones_funcaddrs")?;

    // Enable the verifier if assertions are enabled. Otherwise leave it disabled,
    // as it's quite slow.
    if !cfg!(debug_assertions) {
        sb.set("enable_verifier", "false")?;
    }

    // Baldrdash does its own stack overflow checks, so we don't need Cranelift doing any for us.
    sb.set("probestack_enabled", "false")?;

    // Let's optimize for speed.
    sb.set("opt_level", "best")?;

    Ok(settings::Flags::new(sb))
}
