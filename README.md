# Welcome to Noraneko Browser Repository

<p align="center">
<img src=".github/assets/readme/logo_with_wordmark_light.svg#gh-light-mode-only" width="400px"></img>
<img src=".github/assets/readme//logo_with_wordmark_dark.svg#gh-dark-mode-only" width="400px"></img>
</p>

> [!WARNING]
> Experimental!

Noraneko Browser is currently testbed for Floorp 12.

Have a nice day!

## Contributing

Thank you for investing your time. \
Please check [CONTRIBUTING.md](.github/CONTRIBUTING.md)

## Noraneko Docs available!

Please visit [noraneko.pages.dev](https://noraneko.pages.dev)!

## How to Start Development

1. Run `pnpm install`

You can run `pnpm build` and `pnpm dev`.
`pnpm build` outputs files, while `pnpm dev` is used for debugging the code with file watch.
Refer to "How to Debug" for instructions on using `pnpm dev`.

## How to debug

### Windows

1. Install `gh cli` from [https://cli.github.com/]
2. Visit [noraneko-runtime Action](https://github.com/nyanrus/noraneko-runtime/actions/workflows/wrapper_windows_build.yml).
3. Go to latest successful build and check the id in url.
   It is run_id and is number.
   the `Release` version is recommended.
4. Run `gh run download -R nyanrus/noraneko-runtime -n noraneko-win-amd64-dev [run_id]`
5. Run `pnpm dev`.
6. The browser will launch, and if you change some files, you could rerun `pnpm dev`.
  If the source you changed supports HMR, it will reload the browser so you'll not need to rerun.

### GNU/Linux

1. Install `gh cli` from [https://cli.github.com/] and `lbzip2` from your package manager.
2. Visit [noraneko-runtime Action](https://github.com/nyanrus/noraneko-runtime/actions/workflows/wrapper_linux_build.yml).
3. Go to latest successful build and check the id in url.
   It is run_id and is number.
4. Run `gh run download -R nyanrus/noraneko-runtime -n noraneko-linux-amd64-dev [run_id]`
5. Run `mkdir -p _dist/bin`
6. Run `tar --strip-components=1 -xvf ./noraneko-*.tar.bz2 -C _dist/bin`
7. Run `pnpm dev`
8. The browser will launch, and if you change some files, you could rerun `pnpm dev`.
  If the source you changed supports HMR, it will reload the browser so you'll not need to rerun.

## Credits

Thank you [@CutterKnife](https://github.com/CutterKnife) for the logo!

### Projects that are inspired by or used in Noraneko

- Mozilla Firefox

  License:  \
  [Homepage: mozilla.org](https://www.mozilla.org/en-US/firefox/new/)

- Ablaze Floorp

  License: Mozilla Public License 2.0 \
  [Homepage: floorp.app](https://floorp.app) \
  [GitHub: Floorp-Projects/Floorp](https://github.com/Floorp-Projects/Floorp)

- Fushra Pulse

  License: Mozilla Public License 2.0 \
  [Homepage: pulsebrowser.app](https://pulsebrowser.app/) \
  [GitHub: pulse-browser/browser](https://github.com/pulse-browser/browser)

- Lepton Designs (Firefox-UI-Fix)

  License: Mozilla Public License 2.0 \
  [GitHub: black7375/Firefox-UI-Fix](https://github.com/black7375/Firefox-UI-Fix)

Thank you!

## Useful Links

[![Link to Noraneko Runtime Repository](.github/assets/readme/Link2RuntimeRepo.svg)](https://github.com/nyanrus/noraneko-runtime/)

## LICENSE

Mozilla Public License 2.0
