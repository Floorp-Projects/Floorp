##  Overview

- Changes should follow the [design guidelines], as well as the [coding style guide]
- All changes need tests
- In order to land, changes need a review by one of the Jetpack reviewers
- Changes may need an API review
- Changes may need a review from a Mozilla platform domain-expert

If you have questions, ask in [#jetpack on IRC][jetpack irc channel] or on the [Jetpack mailing list].

## How to Make Code Contributions

Follow the [standard mozilla contribution guidelines](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Introduction). All contributions and patches should be through Bugzilla.

Pull requests on github are not accepted and new pull requests on github will be rejected.

## Good First Bugs

There is a list of [good first bugs here][good first bugs].

## Reviewers

Changes should be reviewed by someone on the [add-on sdk review team](https://bugzilla.mozilla.org/page.cgi?id=review_suggestions.html#Add-on%20SDK) within Bugzilla.

Others who might be able to help include:

- [@mossop]
- [@gozala]
- [@ZER0]
- [@jsantell]
- [@zombie]

For review of Mozilla platform usage and best practices, ask [@autonome],
[@0c0w3], or [@mossop] to find the domain expert.

For API and developer ergonomics review, ask [@gozala].

[design guidelines]:https://wiki.mozilla.org/Labs/Jetpack/Design_Guidelines
[jetpack irc channel]:irc://irc.mozilla.org/#jetpack
[Jetpack mailing list]:http://groups.google.com/group/mozilla-labs-jetpack
[open bugs]:https://bugzilla.mozilla.org/buglist.cgi?quicksearch=product%3ASDK
[make bug]:https://bugzilla.mozilla.org/enter_bug.cgi?product=Add-on%20SDK&component=general
[test intro]:https://developer.mozilla.org/en-US/Add-ons/SDK/Tutorials/Unit_testing
[test API]:https://developer.mozilla.org/en-US/Add-ons/SDK/Low-Level_APIs/test_assert
[coding style guide]:https://github.com/mozilla/addon-sdk/wiki/Coding-style-guide
[Add-on SDK repo]:https://github.com/mozilla/addon-sdk
[GitHub]:https://github.com/
[good first bugs]:https://bugzilla.mozilla.org/buglist.cgi?list_id=7345714&columnlist=bug_severity%2Cpriority%2Cassigned_to%2Cbug_status%2Ctarget_milestone%2Cresolution%2Cshort_desc%2Cchangeddate&query_based_on=jetpack-good-1st-bugs&status_whiteboard_type=allwordssubstr&query_format=advanced&status_whiteboard=[good%20first%20bug]&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&bug_status=VERIFIED&product=Add-on%20SDK&known_name=jetpack-good-1st-bugs

[@mossop]:https://github.com/mossop/
[@gozala]:https://github.com/Gozala/
[@ZER0]:https://github.com/ZER0/
[@jsantell]:https://github.com/jsantell
[@zombie]:https://github.com/zombie
