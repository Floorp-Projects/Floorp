# Sending your code for review (also known as "sending patches")

First, commit your changes. For example:

```bash
hg add /path/to/file/changed
hg commit -m "Bug 1234567 - [devtools] Implement feature XYZ. r=name,name2!"
```

 The commit message explained in detail:
 - `Bug 1234567` - The number of the bug in bugzilla.
 - `- [devtools] Implement feature XYZ.` - The commit message, with a "devtools" prefix to quickly identify DevTools changesets.
 - `r=name` - The short form to request a review. Enter the name you found using the
 instructions in the [previous step](./code-reviews-find-reviewer.md).
 - `,name2!` - You can have more than one reviewer. The `!` makes the review a *blocking* review (Patch can not land without accepted review).

Then create a revision in Phabricator using `moz-phab`:

```bash
moz-phab submit
```

A revision will be created including that information and the difference in code between your changes and the point in the repository where you based your work (this difference is sometimes called "a patch", as it's what you'd need to apply on the repository to get to the final state).

If you click on the provided URL for the revision, it'll bring you to Phabricator's interface, which the reviewer will visit as well in order to review the code. They will look at your changes and decide if they need any additional work, based on whether the changes do fix what the bug describes or not. To get a better idea of the types of things they will look at and verify, read the [code reviews checklist](./code-reviews-checklist.md).

For more information on using moz-phab, you can run:

```bash
moz-phab -h
```

or to get information on a specific command (here `submit`):

```bash
moz-phab submit -h
```

The reviewer might suggest you do additional changes. For example, they might recommend using a helper that already exists (but you were not aware of), or they might recommend renaming things to make things clearer. Or they might recommend you do *less* things (e.g. if you changed other things that are out of scope for the bug). Or they might simply ask questions if things aren't clear. You can also ask questions if the comments are unclear or if you're unsure about parts of the code you're interacting with. Something that looks very obvious to one person might confuse others.

Hence, you might need to go back to the code and do some edits to address the issues and recommendations. Once you have done this, you must update the existing commit:

```bash
hg commit --amend
```

And submit the change again:

```bash
moz-phab submit
```

You might have to go through this cycle of submitting changes and getting it reviewed several times, depending on the complexity of the bug.

Once your code fixes the bug, and there are no more blocking issues, the reviewer will approve the changes, and the code can be landed in the repository now.


# Squashing commits

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
