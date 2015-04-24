##  Overview

- Changes should follow the [design guidelines], as well as the [coding style guide]
- All changes need tests
- In order to land, changes need a review by one of the Jetpack reviewers
- Changes may need an API review
- Changes may need a review from a Mozilla platform domain-expert

If you have questions, ask in [#jetpack on IRC][jetpack irc channel] or on the [Jetpack mailing list].

## How to Make Code Contributions

To write code for the Jetpack project, follow these steps:

1. Look for your issue in the list of [bugs already filed][open bugs]. If you don't already know what you want to do, we keep a list of [good first bugs].
2. If no bug exists, [make one][make bug].
3. Get the code: get a [GitHub][GitHub] account, fork the [Add-on SDK repo][Add-on SDK repo], and clone it to your machine.
4. Make your changes. Changes should follow the [design guidelines] as well as the [coding style guide].
5. Write tests: [unit testing introduction][test intro], [unit testing API][test API].
6. Submit a pull request with the changes and a title in the form of `Bug XXX - description`.
7. Make sure that [Travis CI](https://travis-ci.org/mozilla/addon-sdk/branches) tests are passing for your branch.
8. Copy the pull request link from GitHub and paste it in as an attachment to the bug.
9. Each pull request should ideally contain only one commit, so squash the commits if necessary.
10. Flag the attachment for code review from one of the Jetpack reviewers listed below. This step is optional, but could speed things up.
11. Address any issues mentioned in the review.

Finally, once the review is positive, a team member will do the merging

## Good First Bugs

There is a list of [good first bugs here][good first bugs].

## Reviewers

All changes need a review by someone on the Jetpack review crew:

- [@mossop]
- [@gozala]
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
[@erikvold]:https://github.com/erikvold/
[@jsantell]:https://github.com/jsantell
[@zombie]:https://github.com/zombie
