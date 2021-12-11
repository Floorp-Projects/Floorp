@echo off
REM See comment in cargo-linker (without extension)
%MOZ_CARGO_WRAP_HOST_LD% %MOZ_CARGO_WRAP_HOST_LDFLAGS% %*
