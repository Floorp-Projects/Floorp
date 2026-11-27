# Welcome to Floorp Contributing Guidelines

Thank you for your interest!

First, please ask "how to contribute" in [GitHub Discussions](https://github.com/Floorp-Projects/Floorp/discussions/new?category=q-a) or [Floorp Projects Discord](https://floorp.app/discord) \
It can be helpful if you are stuck, or for communication to start contributing.

You might also find [this documentation](https://docs.floorp.app/docs/contributing/floorp) helpful.

When you ask, make a discussion and make pull request, please use 5W1H as a reference for description.

1. Who
2. When
3. Where
4. What
5. How
6. Why

e.g.
1. NyanRus
2. Two weeks ago
3. src/apps/experiment/csk
4. Custom Shortcut Keys
5. wanted to improve User Experience with changing some UI
6. that was inconvenient because too many information in screen

Please add MPL 2.0 headers to new files you submit, where appropriate. For example, a PNG or JPEG would not require an individual notice. A sample of some headers is available at: https://www.mozilla.org/en-US/MPL/headers/.

## Localization

Floorp's internationalization (i18n) pipeline consists of the main repository, the [`f18n-central`](https://github.com/Floorp-Projects/f18n-central) synchronization hub, and Crowdin.

### Updating English (en-US) Strings
If you find outdated descriptions, grammatical issues, or typos in English (US), **submit a Pull Request to this repository (`Floorp-Projects/Floorp`)**.

Do not edit `f18n-central` directly, as changes there will be overwritten by automated synchronization.

English source files are located as follows:
- **Core browser UI:** `i18n/en-US/*.json` (e.g., `browser-chrome.json`)
- **Windows Installer:** `static/installers/stub-win64-installer/src/lib/i18n/locales/en-US.json`
- **Feature pages (Settings, New tab, etc.):** `browser-features/pages-{featureName}/src/lib/i18n/locales/en-US.json`
  - Examples: `browser-features/pages-settings/...`, `browser-features/pages-newtab/...`, `browser-features/pages-aboutDialog/...`

### Translating into Other Languages
If you want to contribute translations for other languages:
- **Do not edit translation files in GitHub directly.**
- Please contribute via our **[Crowdin project](https://crowdin.com/project/floorp-browser)**. Translations made on Crowdin are automatically imported into Floorp.

For browser and documentation translation guidance, see [Contribute to Floorp](https://docs.floorp.app/docs/contributing/floorp#improve-translations).

Thank you for reading. Have a nice day!

*Updated from the original Noraneko Contribution Guidelines.*
