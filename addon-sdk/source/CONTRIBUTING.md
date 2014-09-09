##  Overview

- Changes should follow the [design guidelines], as well as [coding style guide] for Jetpack
- All changes must be accompanied by tests
- In order to land, changes must have review from a core Jetpack developer
- Changes should have additional API review when needed
- Changes should have additional review from a Mozilla platform domain-expert when needed

If you have questions, ask in [#jetpack on IRC][jetpack irc channel] or on the [Jetpack mailing list].

## How to Make Code Contributions

If you have code that you'd like to contribute the Jetpack project, follow these steps:

1. Look for your issue in the [bugs already filed][open bugs]
2. If no bug exists, [submit one][submit bug]
3. Make your changes, per the Overview
4. Write a test ([intro][test intro], [API][test API])
5. Submit pull request with changes and a title in a form of `Bug XXX - description`
6. Copy the pull request link from GitHub and paste it in as an attachment to the bug
7. Each pull request should idealy contain only one commit, so squash the commits if necessary.
8. Flag the attachment for code review from one of the Jetpack reviewers listed below.
   This step is optional, but could speed things up.
9. Address any nits (ie style changes), or other issues mentioned in the review.
10. Finally, once review is approved, a team member will do the merging

## Good First Bugs

There is a list of [good first bugs here](https://bugzilla.mozilla.org/buglist.cgi?list_id=7345714&columnlist=bug_severity%2Cpriority%2Cassigned_to%2Cbug_status%2Ctarget_milestone%2Cresolution%2Cshort_desc%2Cchangeddate&query_based_on=jetpack-good-1st-bugs&status_whiteboard_type=allwordssubstr&query_format=advanced&status_whiteboard=[good%20first%20bug]&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&bug_status=VERIFIED&product=Add-on%20SDK&known_name=jetpack-good-1st-bugs).

## Reviewers

All changes must be reviewed by someone on the Jetpack review crew:

- [@mossop]
- [@gozala]
- [@wbamberg]
- [@ZER0]
- [@erikvold]
- [@jsantell]
- [@zombie]

For review of Mozilla platform usage and best practices, ask [@autonome],
[@0c0w3], or [@mossop] to find the domain expert.

For API and developer ergonomics review, ask [@gozala].

[design guidelines]:https://wiki.mozilla.org/Labs/Jetpack/Design_Guidelines
[jetpack irc channel]:irc://irc.mozilla.org/#jetpack
[Jetpack mailing list]:http://groups.google.com/group/mozilla-labs-jetpack
[open bugs]:https://bugzilla.mozilla.org/buglist.cgi?quicksearch=product%3ASDK
[submit bug]:https://bugzilla.mozilla.org/enter_bug.cgi?product=Add-on%20SDK&component=general
[test intro]:https://jetpack.mozillalabs.com/sdk/latest/docs/#guide/implementing-reusable-module
[test API]:https://jetpack.mozillalabs.com/sdk/latest/docs/#module/api-utils/unit-test
[coding style guide]:https://github.com/mozilla/addon-sdk/wiki/Coding-style-guide

[@mossop]:https://github.com/mossop/
[@gozala]:https://github.com/Gozala/
[@wbamberg]:https://github.com/wbamberg/
[@ZER0]:https://github.com/ZER0/
[@erikvold]:https://github.com/erikvold/
[@jsantell]:https://github.com/jsantell
[@zombie]:https://github.com/zombie
