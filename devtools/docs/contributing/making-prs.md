# Creating and sending patches <!--TODO: (in the future: Making Pull Requests)-->

<!-- TODO: this will need to be updated in the future when we move to GitHub -->

## Creating a patch

To create a patch you need to first commit your changes and then export them to a patch file.

With Mercurial:
* `hg commit -m 'your commit message'`
* `hg export > /path/to/your/patch`

With Git, the process is similar, but you first need to add an alias to create Mercurial-style patches. Have a look at [the detailed documentation](https://developer.mozilla.org/en-US/docs/Tools/Contributing#Creating_a_patch_to_check_in).

## Commit messags

Commit messages should follow the pattern `Bug 1234567 - change description. r=reviewer`

First is the bug number related to the patch. Then the description should explain what the patch changes. The last part is used to keep track of the reviewer for the patch.

## Submitting a patch

Once you have a patch file, add it as an attachment to the Bugzilla ticket you are working on and add the `feedback?` or `review?` flag depending on if you just want feedback and confirmation you're doing the right thing or if you think the patch is ready to land respectively. Read more about [how to submit a patch and the Bugzilla review cycle here](https://developer.mozilla.org/en-US/docs/Developer_Guide/How_to_Submit_a_Patch).

You can also take a look at the [Code Review Checklist](./code-reviews.md) as it contains a list of checks that your reviewer is likely to go over when reviewing your code.
