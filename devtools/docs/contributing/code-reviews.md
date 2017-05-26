# Code reviews

This checklist is primarily aimed at reviewers, as it lists important points to check while reviewing a patch.

It can also be useful for patch authors: if the changes comply with these guidelines, then it's more likely the review will be approved.

## Bug status and patch file
<!--TODO: update when we move to github-->
* Bug status is assigned, and assignee is correctly set.
* Commit title and message follow [the conventions](https://developer.mozilla.org/en-US/docs/Mercurial/Using_Mercurial#Commit_Message_Conventions).
* Commit message says [what is being changed and why](http://mozilla-version-control-tools.readthedocs.org/en/latest/mozreview/commits.html#write-detailed-commit-messages).
* Patch applies locally to current sources with no merge required.
* Check that every new file introduced by the patch has the proper Mozilla license header: https://www.mozilla.org/en-US/MPL/headers/

## Manual testing

* Verify:
  * if it's a new feature, the patch implements it.
  * if it's a fix, the patch fixes the bug it addresses.
* Report any problems you find in the global review comment.
* Decide if any of those problems should block landing the change, or if they can be filed as follow-up bugs instead, to be fixed later.

## Automated testing

* Run new/modified tests, [with and without e10s](../tests/writing-tests.md#electrolysis).
* Watch out for tests that pass but log exceptions or end before protocol requests are handled.
* Watch out for slow/long tests: suggest many small tests rather than single long tests.
* Watch out for new tests written as integration tests instead of as unit tests: unit tests should be the preferred option, when possible.

## Code review

* Code changes:
 * Review only what was changed by the contributor.
 * Code formatting follows [our ESLint rules](eslint.md) and [coding standards](./coding-standards.md).
 * Code is properly commented, JSDoc is updated, new "public" methods all have JSDoc, see the [comment guidelines](./javascript.md#comments).
 * If Promise code was added/modified, the right promise syntax is used and rejections are handled. See [asynchronous code](./javascript.md#asynchronous-code).
 * If a CSS file is added/modified, it follows [the CSS guidelines](./css.md).
 * If a React or Redux module is added/modified, it follows the [React/Redux guidelines](./javascript.md#react--redux).
 * If DevTools server code that should run in a worker is added/modified then it shouldn't use Services
* Test changes:
 * The feature or bug is [tested by new tests, or a modification of existing tests](../tests/writing-tests.md).
 * [Test logging](../tests/writing-tests.md#logs-and-comments) is sufficient to help investigating test failures/timeouts.
 * [Test is e10s compliant](../tests/writing-tests.md#e10s-electrolysis) (no CPOWs, no content, etc…).
 * Tests are [clean and maintainable](../tests/writing-tests.md#writing-clean-maintainable-test-code).
 * A try push has started (or even better, is green already)<!--TODO review and update with mentions to Travis, Circle or whatever it is we use when we move to GitHub-->.
* User facing changes:
 * If a new piece of UI or new user interaction is added/modified, then UX is `ui-review?` on the bug.<!--TODO this needs updating with the new process-->
 * If a user facing string has been added, it is localized and follows [the localization guidelines](../files/adding-files.md#localization-l10n).
 * If a user-facing string has changed meaning, [the key has been updated](https://developer.mozilla.org/en-US/docs/Mozilla/Localization/Localization_content_best_practices#Changing_existing_strings).
 * If a new image is added, it is a SVG image or there is a reason for not using a SVG.
 * If a SVG is added/modified, it follows [the SVG guidelines](../frontend/svgs.md).
 * If a documented feature has been modified, the keyword `dev-doc-needed` is present on the bug.

## Finalize the review
<!--TODO update with the GitHub workflow when we're there-->
* R+: the code should land as soon as possible.
* R+ with comments: there are some comments, but they are minor enough, or don't require a new review once addressed, trust the author.
* R cancel / R- / F+: there is something wrong with the code, and a new review is required.

