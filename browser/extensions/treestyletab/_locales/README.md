# Notes for translators

## Policy about translation (See also [#2434](https://github.com/piroor/treestyletab/pull/2434))

* You should not translate the name of this addon itself "Tree-shaped vertical tabs", instead you can add a translated name after that like "Tree-shaped vertical tabs - ツリー型垂直タブ" (this just repeats "Tree-shaped vertical tabs" in Japanese).
  * This is for better findability on the Mozilla Add-ons website and the addons manager.
  * Here is [the commit to revert localized addon name to the global name](https://github.com/piroor/treestyletab/commit/73cee9a81882b1f09de149e8a549620b24fe31e7).
    The term "Tree-shaped vertical tabs" in these messages means this addon itself.
* You still can translate the title of the sidebar panel "Tree-shaped vertical tabs" in your language, for more natural user experience.

## Translation on a web service

You can translate TST's messages on [GitLocalize.com](https://gitlocalize.com/repo/6376). If you want to become a moderator of a language, please sign up to the service and contact me.

Even if you don't have an account on GitLocalize.com, the [Web Extension Translator](https://lusito.github.io/web-ext-translator/) will help your translation. Steps:

1. Click the "Load from GitHub" button (an octcat icon).
2. Input an URL `https://github.com/piroor/treestyletab/tree/trunk` in to the field and click the "OK" button.
3. Choose your language from the rightside dropdown list.
4. Find fields marked with red line - blank (untranslated) or updated messages, and translate them.
5. Click the "Submit changes to the developers" button (a right arrow icon).
6. Choose your language again and click the "OK" button.
7. Then a new issue becomes ready to be created and your translation result is copied to the clipboard.
   *But please don't create an issue for now.*
8. Log in to the GitHub.
9. Go to the resource `webextensions/_locales/(language code)/messages.json` on this repository.
   For example [`webextensions/_locales/ja/messages.json`](https://github.com/piroor/treestyletab/blob/master/webextensions/_locales/ja/messages.json) for Japanese language.
10. Click the "Edit" button (an pencil icon). Then you'll see an edit form for the file.
11. Select all, delete it, and paste your translation from the clipboard to the form.
12. Remove needless prefix and suffix: `'''json` and `'''` (three backquotes actually).
13. Input a commit message like `Update Japanese translation` into the input field below the heading "Propose file change".
14. Click the "Propose file change" button. Then you'll see difference of the change.
15. Click the "Create pull request" button. Then you'll see a form to create a new pull request.
16. Fill the title and the comment fields, and click the "Create pull request" button. Then a pull request is really created and a notification message will be sent to me automatically.

## Translation on your local environment with a text editor

There are some helper utilities for translations.

 * [ChromeExtensionI18nHelper for Sublime Text plug-in](https://github.com/Harurow/sublime_chromeextensioni18nhelper): it is designed for Google Chrome extensions, but it is also available WebExtensions-based Firefox addons.
 * [web-ext-translator](https://www.npmjs.com/package/web-ext-translator): a Java-based web editor for translator UI. Offline version of the [Web Extension Translator](https://lusito.github.io/web-ext-translator/).

If you want to know changes in the main `en` locale, you can compare the latest code with any specific version. For example, changes from the version 3.2.0 is: [https://github.com/piroor/treestyletab/compare/3.2.0...master](https://github.com/piroor/treestyletab/compare/3.2.0...master)

Sadly GitHub doesn't provide ability to show the difference between two revisions about single file, but you can see that with the raw `git` command. For example if you want to see changes of the English resource from the version 3.2.0 to the latest revision:

```bash
$ git diff 3.2.0 master -- webextensions/_locales/en/messages.json
```
