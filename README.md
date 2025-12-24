# Floorp Browser 👋

<div align="center">
  <a href="https://github.com/Floorp-Projects/Floorp">
    <img src="https://avatars.githubusercontent.com/u/94953125?s=200&v=4" alt="Logo" width="150" height="150">
  </a>

  <h3 align="center">Floorp Browser</h3>

  <p align="center">
    A Browser built for keeping the Open, Private and Sustainable Web alive. Based on Mozilla Firefox.
    <br />
    <br />
    <a href="https://floorp.app">Official Site</a>
    ・
    <a href="https://floorp.app/download">Download</a>
    ・
    <a href="https://blog.floorp.app">Blog & Release Notes</a>
    ・
    <a href="https://discord.floorp.app">Official Discord</a>
    ・
    <a href="https://docs.floorp.app">Documentation</a>
  </p>

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![Crowdin](https://badges.crowdin.net/floorp-browser/localized.svg)](https://crowdin.com/project/floorp-browser)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Floorp-Projects/Floorp)

</div>

---

## 📄 Sponsorship

Floorp Browser is a free and open-source project. If you like Floorp Browser, please consider sponsoring us. Your sponsorship will help us to continue development and pay for the server costs.

- [GitHub Sponsors](https://github.com/sponsors/surapunoyousei)

<h2> 💕 Special Sponsors </h2>
<div align="left" style="display: flex; flex-wrap: wrap; gap: 20px;">
  <a href="https://www.cube-soft.jp/" style="text-decoration: none; color: inherit; text-align: center;">
    <img src="https://avatars.githubusercontent.com/u/346808?s=200&v=4" alt="CubeSoft, Inc." width="100" height="100">
    <br><b>CubeSoft, Inc.</b>
  </a>
  <a href="https://signpath.io/" style="text-decoration: none; color: inherit; text-align: center;">
    <img src="https://github.com/signpath.png" alt="SignPath" width="100" height="100">
    <br><b>SignPath</b>
  </a>
  <a href="https://1password.com/" style="text-decoration: none; color: inherit; text-align: center;">
    <img src="https://github.com/1Password.png" alt="1Password" width="100" height="100">
    <br><b>1Password</b>
  </a>
</div>

---

## 📂 Project Structure

- `bridge/`: Core logic for connecting browser features and the browser engine.
- `browser-features/`: Implementation of various browser features.
  - `chrome/`: Core browser logic and UI components.
  - `pages-*/`: Specific browser pages like Settings, New Tab, and Profile Manager.
- `i18n/`: Localization files for multiple languages.
- `libs/`: Shared libraries, type definitions, and Vite plugins.
- `static/`: Static assets, Gecko preferences, and installers.
- `tools/`: Build scripts, patches, and development utilities.

---

## ⚡ Getting Started

### 💻 Supported Operating Systems

- **Windows**: 10 or later (x86_64)
- **macOS**: 10.15 or later (Universal: x86_64 & ARM64)
- **Linux**: Most distributions (x86_64 & AArch64)

### 🧰 How to Start Development

1.  **Install Deno**: [https://deno.land/](https://deno.land/)
2.  **Clone Repository**: `git clone https://github.com/Floorp-Projects/Floorp.git`
3.  **Install Dependencies**: `deno install`
4.  **Run Development Task**: `deno task feles-build dev`
    - At first run, the necessary binaries will be downloaded.
    - A browser will launch with hot-reload enabled.

#### Other Build Commands

- `deno task feles-build build`: Run a full production build.
- `deno task feles-build stage`: Run a staged production build with the browser in development mode.

---

## 📝 License & Privacy

- **License**: [Mozilla Public License 2.0](https://www.mozilla.org/en-US/MPL/2.0/)
- **Privacy Policy**: [Floorp Privacy Policy](https://floorp.app/privacy)

Floorp Browser is based on Mozilla Firefox. Floorp Browser is not affiliated with Mozilla & Mozilla Firefox.

---

## 📄 Floorp License Notices

Floorp utilizes various open-source projects. Below is a list of the major open-source projects used:

- **Mozilla Firefox**: [MPL 2.0](https://www.mozilla.org/en-US/MPL/2.0/)
- **NyanRus Noraneko**: [MPL 2.0](https://github.com/nyanrus/noraneko-runtime/blob/main/LICENSE)
- **Firefox UI FIX (Lepton)**: [MPL 2.0](https://github.com/black7375/Firefox-UI-Fix/blob/master/LICENSE)
- **userChromeCSS Loader**: MIT (Author: Griever)
- **Paxmod**: MIT (Author: numirias)
- **Fushra Pulse**: [MPL 2.0](https://github.com/pulse-browser/browser/blob/main/LICENSE)

---

## 📧 Contact

- [Official Twitter](https://twitter.com/Floorp_Browser)
- [Official Discord](https://discord.floorp.app)

---

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=Floorp-Projects/Floorp&type=date&legend=top-left)](https://www.star-history.com/#Floorp-Projects/Floorp&type=date&legend=top-left)

<!-- MARKDOWN LINKS & IMAGES -->

[contributors-shield]: https://img.shields.io/github/contributors/Floorp-Projects/Floorp.svg?style=for-the-badge
[contributors-url]: https://github.com/Floorp-Projects/Floorp/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/Floorp-Projects/Floorp?style=for-the-badge
[forks-url]: https://github.com/Floorp-Projects/Floorp/network/members
[stars-shield]: https://img.shields.io/github/stars/Floorp-Projects/Floorp.svg?style=for-the-badge
[stars-url]: https://github.com/Floorp-Projects/Floorp/stargazers
[issues-shield]: https://img.shields.io/github/issues/Floorp-Projects/Floorp.svg?style=for-the-badge
[issues-url]: https://github.com/Floorp-Projects/Floorp-Projects/issues
