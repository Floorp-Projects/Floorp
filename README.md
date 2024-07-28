# Welcome to Noraneko Browser Repository!

<p align="center">
<img src="docs/assets/logo_with_wordmark_light.svg#gh-light-mode-only" width="400px"></img>
<img src="docs/assets/logo_with_wordmark_dark.svg#gh-dark-mode-only" width="400px"></img>
</p>

> [!WARNING]
> Experimental!

Noraneko Browser is currently testbed of modern JS environment for Floorp.

Star me!
## How to Start Development

1. Run `pnpm install`

You can run `pnpm build` and `pnpm dev`.
`pnpm build` outputs files, while `pnpm dev` is used for debugging the code with file watch.
Refer to "How to Debug" for instructions on using `pnpm dev`.

# How to Debug

### Windows

1. Install zstd and add it to the PATH.
2. Download the latest successful build artifact from [noraneko-runtime Action](https://github.com/nyanrus/noraneko-runtime/actions/workflows/wrapper_windows_build.yml).
3. Extract the zip file and place `nora-win_x64-bin.tar.zst` in the project root.
4. Run `pnpm dev`.
5. The browser will launch, and if you change any files, the browser will restart automatically.

## Credits

Thank you [@CutterKnife](https://github.com/CutterKnife) for the logo!

### Projects that are inspired by or used in Noraneko

[Mozilla Firefox](https://www.mozilla.org/en-US/firefox/new/)
[Ablaze Floorp](https://github.com/Floorp-Projects/Floorp)
[Fushra Pulse Browser](https://github.com/pulse-browser/browser)
[Lepton Designs](https://github.com/black7375/Firefox-UI-Fix)

Thank you!

## LICENSE
Mozilla Public License 2.0
