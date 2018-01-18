# Creating and sending patches <!--TODO: (in the future: Making Pull Requests)-->

<!-- TODO: this will need to be updated in the future when we move to GitHub -->

## Creating a patch

To create a patch you need to first commit your changes and then export them to a patch file.

With Mercurial:
* `hg commit -m 'your commit message'`
* `hg export > /path/to/your/patch`

With Git, the process is similar, but you first need to add an alias to create Mercurial-style patches. Have a look at [the detailed documentation](https://developer.mozilla.org/en-US/docs/Tools/Contributing#Creating_a_patch_to_check_in).

## Commit messages

Commit messages should follow the pattern `Bug 1234567 - change description. r=reviewer`

First is the bug number related to the patch. Then the description should explain what the patch changes. The last part is used to keep track of the reviewer for the patch.

## Submitting a patch

Once you have a patch file, add it as an attachment to the Bugzilla ticket you are working on and add the `feedback?` or `review?` flag depending on if you just want feedback and confirmation you're doing the right thing or if you think the patch is ready to land respectively. Read more about [how to submit a patch and the Bugzilla review cycle here](https://developer.mozilla.org/en-US/docs/Developer_Guide/How_to_Submit_a_Patch).

You can also take a look at the [Code Review Checklist](./code-reviews.md) as it contains a list of checks that your reviewer is likely to go over when reviewing your code.

## Squashing commits

Sometimes you may be asked to squash your commits. Squashing means merging multiple commits into one in case you created multiple commits while working on a bug. Squashing bugs is easy with steps listed below for both git and mercurial.

### With Mercurial:

We will use the histedit extension for squashing commits in Mercurial. You can check if this extension is enabled in your Mercurial following these steps:
* Open `.hgrc` (default configuration file of mercurial) located in the home directory on Linux/OSX, using your favourite editor.
* Then add `histedit= ` under the `[extensions]` list present in file, if not present already.

Then, run the following command:

`hg histedit`

You will see something like this on your terminal:

```
pick 3bd22d1cc59a 0 "First-Commit-Message"
pick 81c4d40e57d3 1 "Second-Commit-Message"
```

These lines represent your commits. Suppose we want to merge 81c4d40e57d3 to 3bd22d1cc59a. Then replace **pick** in front of 81c4d40e57d3 with **fold** (or simply 'f'). Save the changes.

You will see that 81c4d40e57d3 has been combined with 3bd22d1cc59a. You can verify this using `hg log` command.

You can use fold to as many commits you want and they will be combined with the first commit above them which does not use fold.

### With Git:

To squash commits in git we will use the rebase command. Let's learn the syntax using an example:

`git rebase -i HEAD~2`

Above commands is for rebasing commits.
* The -i flag is to run rebase in interactive mode.
* HEAD~2 is to rebase the last two commits.

You can use any number, depending on how many commits you want to rebase. 

After you run that command, an editor will open displaying contents similar to this:

```
pick 5878025 "First-Commit-Message"
pick bd1efe7 "Second-Commit-Message"
```

Suppose you want to merge `bd1efe7` to `5878025`, then replace **pick** with **squash** (or simply 's') and save the changes and close the editor, then a file should open like this:

```
    # This is a combination of 2 commits.
    # The first commit's message is:

    < First-Commit-Message >

    This is the 2nd commit message:

    < Second-Commit-Message >

```
Write a new commit message (note that this will be a new commit message used for squashed commits). Every line not starting with # above contributes to the commit message.

Save your changes and you are done. You will see that your commits have been squashed and you can verify this using `git log` or `git reflog`.
