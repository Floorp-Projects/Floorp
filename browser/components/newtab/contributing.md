# Contributing to Activity Stream

Activity Stream is an enhancement to the functionality of Firefox's about:newtab page.  We welcome new 'streamers' to contribute to the project!

## Where to ask questions

- Most of the core dev team can be found on the `#activity-stream` channel on `irc.mozilla.org`.
  You can also direct message the core team (`dmose`, `emtwo`, `jkerim`, `k88hudson`, `Mardak`, `nanj`, `r1cky`, `ursula`, `andreio`)
  or our manager (`tspurway`)
- Slack channel (staff only): #activitystream
- Mailing List: [activity-stream-dev](https://groups.google.com/a/mozilla.com/d/forum/activity-stream-dev)
- File issues/questions on Github: https://github.com/mozilla/activity-stream/issues. We typically triage new issues every Monday.

## Architecture ##

Activity Stream is a Firefox system add-on. One of the cool things about Activity Stream is that the
[content side of the add-on](https://developer.mozilla.org/en-US/Add-ons/SDK/Guides/Content_Scripts)
is written using [ReactJS](https://facebook.github.io/react/).  This makes it an awesome project for React hackers to contribute to!

## Finding Bugs, Filing Tickets, Earning Karma ##

Activity Stream lives on [GitHub](https://github.com/mozilla/activity-stream), but you already knew that!  If you've found
a bug, or have a feature idea that you you'd like to see in Activity Stream, follow these simple guidelines:
- Pick a thoughtful and terse title for the issue (ie. *not* Thing Doesn't Work!)
- Make sure to mention your Firefox version, OS and basic system parameters (eg. Firefox 49.0, Windows XP, 512KB RAM)
- If you can reproduce the bug, give a step-by-step recipe
- Include [stack traces from the console(s)](https://developer.mozilla.org/en-US/docs/Mozilla/Debugging/Debugging_JavaScript) where appropriate
- Screenshots welcome!
- When in doubt, take a look at some [existing issues](https://github.com/mozilla/activity-stream/issues) and emulate

## Take a Ticket, Hack some Code ##

If you are new to the repo, you might want to pay close attention to [`Good first bug`](https://github.com/mozilla/activity-stream/issues?q=is%3Aopen+is%3Aissue+label%3A%22Good+first+bug%22),
 [`Bug`](https://github.com/mozilla/activity-stream/issues?q=is%3Aopen%20is%3Aissue%20label%3ABug%20),
 [`Chore`](https://github.com/mozilla/activity-stream/issues?q=is%3Aopen+is%3Aissue+label%3AChore) and
 [`Polish`](https://github.com/mozilla/activity-stream/issues?q=is%3Aopen+is%3Aissue+label%3APolish) tags, as these are
typically a great way to get started.  You might see a bug that is not yet assigned to anyone, or start a conversation with
an engineer in the ticket itself, expressing your interest in taking the bug.  If you take the bug, someone will set
the ticket to [`Assigned to Contributor`](https://github.com/mozilla/activity-stream/issues?utf8=%E2%9C%93&q=is%3Aopen%20is%3Aissue%20label%3A%22Assigned%20to%20contributor%22%20), which is a way we can be pro-active about helping you succeed in fixing the bug.

When you have some code written, you can open up a [Pull Request](#pull-requests), get your code [reviewed](#code-reviews), and see your code merged into the Activity Stream codebase.

If you are thinking about contributing on a longer-term basis, check out the section on [milestones](#milestones) and [priorities](#priorities)
to get a sense of how we plan and prioritize work.

## Setting up your development environment

Check out [this guide](docs/v2-system-addon/1.GETTING_STARTED.md) on how to install dependencies, get set up, and run tests.

## Pull Requests ##

You have identified the bug, written code and now want to get it into the main repo using a [Pull Request](https://help.github.com/articles/about-pull-requests/).

All code is added using a pull request against the `master` branch of our repo.  Before submitting a PR, please go through this checklist:
- all [unit tests](#unit-tests) must pass
- if you haven't written unit tests for your patch, eyebrows will be curmudgeonly furrowed (write unit tests!)
- if your pull request fixes a particular ticket (it does, right?), please use the `fixes #nnn` github annotation to indicate this
- please add a `PR / Needs review` tag to your PR (if you have permission).  This starts the code review process.  If you cannot add a tag, don't worry, we will add it during triage.
- if you can pick a module owner to be your reviewer by including `r? @username` in the comment (if not, don't worry, we will assign a reviewer)
- make sure your PR will merge gracefully with `master` at the time you create the PR, and that your commit history is 'clean'

### Setting up pre-push hooks

If you contribute often and would like to set up a pre-push hook to always run `npm lint` before you push to Github,
you can run the following from the root of the activity-stream directory:

```
cp hooks/pre-push .git/hooks/pre-push && chmod +x .git/hooks/pre-push
```

Your hook should now run whenever you run `git push`. To skip it, use the `--no-verify` option:

```
git push --no-verify
```


## Code Reviews ##

You have created a PR and submitted it to the repo, and now are waiting patiently for you code review feedback.  One of the projects
module owners will be along and will either:
- make suggestions for some improvements
- give you an `R+` in the comments section, indicating the review is done and the code can be merged

Typically, you will iterate on the PR, making changes and pushing your changes to new commits on the PR.  When the reviewer is
 satisfied that your code is good-to-go, you will get the coveted `R+` comment, and your code can be merged.  If you have
 commit permission, you can go ahead and merge the code to `master`, otherwise, it will be done for you.

Our project prides itself on it's respectful, patient and positive attitude when it comes to reviewing contributor's code, and as such,
we expect contributors to be respectful, patient and positive in their communications as well.  Let's be friends and learn
from each other for a free and awesome web!

[Mozilla Committing Rules and Responsibilities](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Committing_Rules_and_Responsibilities)

## Git Commit Guidelines ##

We loosely follow the [Angular commit guidelines](https://github.com/angular/angular.js/blob/master/CONTRIBUTING.md#type) of `<type>(<scope>): <subject>` where `type` must be one of:

* **feat**: A new feature
* **fix**: A bug fix
* **docs**: Documentation only changes
* **style**: Changes that do not affect the meaning of the code (white-space, formatting, missing
  semi-colons, etc)
* **refactor**: A code change that neither fixes a bug or adds a feature
* **perf**: A code change that improves performance
* **test**: Adding missing tests
* **chore**: Changes to the build process or auxiliary tools and libraries such as documentation
  generation

### Scope
The scope could be anything specifying place of the commit change. For example `timeline`,
`metadata`, `reporting`, `experiments` etc...

### Subject
The subject contains succinct description of the change:

* use the imperative, present tense: "change" not "changed" nor "changes"
* don't capitalize first letter
* no dot (.) at the end

### Body
In order to maintain a reference to the context of the commit, add
`fixes #<issue_number>` if it closes a related issue or `issue #<issue_number>`
if it's a partial fix.

You can also write a detailed description of the commit:
Just as in the **subject**, use the imperative, present tense: "change" not "changed" nor "changes"
It should include the motivation for the change and contrast this with previous behavior.

### Footer
The footer should contain any information about **Breaking Changes** and is also the place to
reference GitHub issues that this commit **Closes**.

## Milestones ##

All work on Activity Stream is broken into two week iterations, which we map into a GitHub [Milestone](https://github.com/mozilla/activity-stream/milestones).  At the beginning of the iteration, we prioritize and estimate tickets
into the milestone, attempting to figure out how much progress we can make during the iteration.  

## Priorities ##

All tickets that have been [triaged](#triage) will have a priority tag of either `P1`, `P2`, `P3`, or `P4` which are highest to lowest
priorities of tickets in Activity Stream. We love ticket tags and you might also see `Blocked`, `Critical` or `Chemspill` tags, which
indicate our level of anxiety about that particular ticket.  

## Triage ##

The project team meets weekly (in a closed meeting, for the time being), to discuss project priorities, to triage new tickets, and to
redistribute the work amongst team members.  Any contributors tickets or PRs are carefully considered, prioritized, and if needed,
assigned a reviewer.  The project's GitHub [Milestone page](https://github.com/mozilla/activity-stream/milestones) is the best
place to look for up-to-date information on project priorities and current workload.

## License

MPL 2.0
