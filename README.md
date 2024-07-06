# Welcome to Noraneko Browser Repository!

<p align="center">
<img src="docs/assets/logo_with_wordmark_light.svg#gh-light-mode-only" width="400px"></img>
<img src="docs/assets/logo_with_wordmark_dark.svg#gh-dark-mode-only" width="400px"></img>
</p>

> [!WARNING]
> Experimental!

Noraneko Browser is currently testbed of modern JS environment for Floorp.

Star me!

## How to start develop

1. `pnpm install`

You can run `pnpm build` and `pnpm dev`  
`pnpm build` is just outputting files, and `pnpm dev` is debugging the code with file watch  
Kindly refer "How to debug" if you want to know how to use `pnpm dev`.

## How to debug

### Windows

1. install zstd and add to PATH
2. Download latest successful build artifact from [noraneko-runtime Action](https://github.com/nyanrus/noraneko-runtime/actions/workflows/wrapper_windows_build.yml)
3. extract the zip and place a nora-win_x64-bin.tar.zst to project root
4. run `pnpm dev`
5. the browser will run and if you change files, the browser will restart.

Thank you [@CutterKnife](https://github.com/CutterKnife) for the logo!

## LICENSE
Mozilla Public License 2.0
