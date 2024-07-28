# @nora/core

This component handles almost all of Noraneko's core code.

If you are to make feature modifying preferences, or other internal html, go to `root/apps/main/about/`.

## Directory Structure

common/

- codes that don't require noraneko-runtime
- almost main code

nora/

- codes that requires noraneko-runtime (esp. cpp patch)
- should be disabled easily by others

utils/

- Utilities that helps you to code comfortable.
- It is preferred that single file but folder with index.ts can be in.

example/

- as test and template for basic codes

temp/

- obsolete files
