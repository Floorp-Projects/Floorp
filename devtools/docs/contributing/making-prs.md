# Creating and sending patches <!--TODO: (in the future: Making Pull Requests)-->

<!-- TODO: this will need to be updated in the future when we move to GitHub -->

Once you have a patch file, add it as an attachment to the Bugzilla ticket you are working on and add the `feedback?` or `review?` flag depending on if you just want feedback and confirmation you're doing the right thing or if you think the patch is ready to land respectively. Read more about [how to submit a patch and the Bugzilla review cycle here](https://developer.mozilla.org/en-US/docs/Developer_Guide/How_to_Submit_a_Patch).

You can also take a look at the [Code Review Checklist](./code-reviews.md) as it contains a list of checks that your reviewer is likely to go over when reviewing your code.

## Posting patches as gists

* Install gist-cli: `npm install -g gist-cli`
* In your bash profile add:
 * git: `alias gist-patch="git diff | gist -o -t patch"`
 * hg: `alias gist-patch="hg diff | gist -o -t patch"`
* Then go to a clean repo, modify the files, and run the command `gist-patch`
