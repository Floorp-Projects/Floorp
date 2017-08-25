use cubeb::{Context, Result};
use std::env;
use std::io::{self, Write};

pub fn init(ctx_name: &str) -> Result<Context> {
    let backend = match env::var("CUBEB_BACKEND") {
        Ok(s) => Some(s),
        Err(_) => None,
    };

    let ctx = Context::init(ctx_name, None);
    if let Ok(ref ctx) = ctx {
        if let Some(ref backend) = backend {
            let ctx_backend = ctx.backend_id();
            if backend != ctx_backend {
                let stderr = io::stderr();
                let mut handle = stderr.lock();

                writeln!(
                    handle,
                    "Requested backend `{}', got `{}'",
                    backend,
                    ctx_backend
                ).unwrap();
            }
        }
    }

    ctx
}
