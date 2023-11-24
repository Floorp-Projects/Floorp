# Landing code (i.e. getting code into Mozilla's repository)

Code changes (patches) in Mozilla are not 'merged' in a sequential way, as it's the fashion in other popular projects. Here, the patches will be *applied* on top of the latest code, and will stay there if

1. the patch applies cleanly, without conflicts
2. the patch doesn't cause 'bustage' (i.e. breaks the build)

Therefore, it's good to try and do smaller changes rather than bigger, specially if you're modifying files that many other people are working on simultaneously, to avoid conflicts and your patch being rejected. Otherwise you might need to rebase from the latest changes, try to write your changes on top of it, and submit this new diff.

Leaving potential conflicts aside, a patch can make its way into the repository in two ways:

## From Phabricator

Once a review has been approved, someone with enough privileges can request the code be merged, using the [Lando](https://moz-conduit.readthedocs.io/en/latest/lando-user.html) interface. These 'privileges' are "commit level access 3". You get these once you have successfully contributed with a number of patches. See [levelling up](https://firefox-source-docs.mozilla.org/contributing/levelling-up.html) for more details.

If you don't have the privileges, you can also ask your mentor to land the code. In fact, they might even initiate that for you once the code review is approved.

To request the landing, ask your reviewer to land the patch.
