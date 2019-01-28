# Sending your code for review (also known as "sending patches")

There are two ways of doing this. We'll first explain the recommended version, and then the alternative, which is older, and we don't recommend, but might be handy in some circumstances.

## Using Phabricator + Differential (RECOMMENDED)

First, commit your changes. For example:

```bash
hg add /path/to/file/changed
hg commit -m "Bug 1234567 - Implement feature XYZ."
```

Then create a revision in Differential, using Arcanist (or `moz-phab`):

```bash
arc diff
```

You'll be taken to an editor to add extra details, although some of them might be prepopulated using the data in the commit message. Make sure the bug number is filled with the right value. You'll also be able to set reviewers here: enter the names you found using the instructions in the [previous step](./code-reviews-find-reviewer.md).

Save the changes and quit the editor. A revision will be created including those data and also the difference in code between your changes and the point in the repository where you based your work (this difference is sometimes called "a patch", as it's what you'd need to apply on the repository to get to the final state).

If you click on the provided URL for the revision, it'll bring you to Phabricator's interface, which the reviewer will visit as well in order to review the code. They will look at your changes and decide if they need any additional work, based on whether the changes do fix what the bug describes or not. To get a better idea of the types of things they will look at and verify, read the [code reviews checklist](./code-reviews-checklist.md).

The reviewer might suggest you do additional changes. For example, they might recommend using a helper that already exists (but you were not aware of), or they might recommend renaming things to make things clearer. Or they might recommend you do *less* things (e.g. if you changed other things that are out of scope for the bug). Or they might simply ask questions if things aren't clear. You can also ask questions if the comments are unclear or if you're unsure about parts of the code you're interacting with. Something that looks very obvious to one person might confuse others.

Hence, you might need to go back to the code and do some edits to address the issues and recommendations. Once you have done this, we want to amend the existing commit:

```bash
hg commit --amend -m 'Address revision issues: rewrite to use helper helpfulHelper()'
```

And submit the change again:

```bash
arc diff
```

This time, the editor that opens should have filled most of the information already. Add any missing information, save and quit the editor.

You might have to go through this cycle of uploading a diff and getting it reviewed several times, depending on the complexity of the bug.

**NOTE**: by default, Arcanist opens nano, which can be a bit confusing if you're not a nano user. You can override this by setting the `EDITOR` env variable. For instance, you could add this to your `.bash_profile` to set Vim as your preferred editor:

```bash
export EDITOR="/usr/local/bin/vim"
```

Once your code fixes the bug, and there are no more blocking issues, the reviewer will approve the review, and the code can be landed in the repository now.

For more information on using Arcanist, please refer to [their user guide](https://moz-conduit.readthedocs.io/en/latest/arcanist-user.html#submitting-and-updating-patches).

## Using Mercurial (not recommended)

We don't recommend to use this method as it's more manual and makes reviewing and landing code a bit more tedious.

### Creating a patch

To create a patch you need to first commit your changes and then export them to a patch file.

```
hg commit -m 'your commit message'
hg export > /path/to/your/patch
```

### Commit messages

Commit messages should follow the pattern `Bug 1234567 - change description. r=reviewer`.

First is the bug number related to the patch. Then the description should explain what the patch changes. The last part is used to keep track of the reviewer for the patch.

### Submitting a patch

Once you have a patch file, add it as an attachment to the Bugzilla ticket you are working on and add the `feedback?` or `review?` flag depending on if you just want feedback and confirmation you're doing the right thing or if you think the patch is ready to land respectively. Read more about [how to submit a patch and the Bugzilla review cycle here](https://developer.mozilla.org/en-US/docs/Developer_Guide/How_to_Submit_a_Patch).

You can also take a look at the [Code Review Checklist](./code-reviews-checklist.md) as it contains a list of checks that your reviewer is likely to go over when reviewing your code.

### Squashing commits

Sometimes you may be asked to squash your commits. Squashing means merging multiple commits into one in case you created multiple commits while working on a bug. Squashing bugs is easy!

We will use the histedit extension for squashing commits in Mercurial. You can check if this extension is enabled in your Mercurial installation following these steps:

* Open `.hgrc` (Linux/OSX) or `Mercurial.ini` (Windows) –this is the default configuration file of Mercurial– located in your home directory, using your favourite editor.
* Then add `histedit= ` under the `[extensions]` list present in file, if not present already.

Then, run the following command:

`hg histedit`

You will see something like this on your terminal:

```
pick 3bd22d1cc59a 0 "First-Commit-Message"
pick 81c4d40e57d3 1 "Second-Commit-Message"
```

These lines represent your commits. Suppose we want to merge `81c4d40e57d3` to `3bd22d1cc59a`. Then replace **pick** in front of `81c4d40e57d3` with **fold** (or simply 'f'). Save the changes.

You will see that `81c4d40e57d3` has been combined with `3bd22d1cc59a`. You can verify this using the `hg log` command.

You can fold as many commits you want, and they will be combined with the first commit above them which does not use fold.
