# Code reviews

A review is required before code is added to Mozilla's repository. In addition, contrary to what you might have seen in other open source projects, code is not pushed directly to the repository. Instead, once the code is approved by reviewers, they will _request_ the code to be _landed_ on the repository.

All this can be done somehow manually, but we have infrastructure in place to help us with reviews and landing code.

Learn about how to get started with getting your code reviewed and landed in our [setting up](./code-reviews-setup.md) guide.

And read on to learn about why and how we do code reviews.

## Why do we do code reviews?

### Quality

Doing code reviews helps with **correctness**. It gives us a chance to check that the code:

- fixes the problem,
- doesn't have bugs,
- doesn't regress other things,
- covers edge cases.

A review is also a chance for the reviewer to check that the right **test coverage** is here, that the change doesn't have **performance** problems, that it **simplifies hard to understand code** and that it comes with **code comments** and **adequate documentation**.

### Learning and sharing knowledge

While going through the process of doing a code review, **both the reviewer and the reviewee** will be learning something (a new part of the code, a new efficient way to write code, tests, localization, etc.).

Making the code easy to understand by the reviewer helps everybody.

Doing reviews also helps people working on DevTools feel more comfortable and learn about new parts of the codebase.

It helps build consensus around ideas and practices.

### Maintenance

Doing reviews gives us an opportunity to check we are willing to maintain and support the new code that being introduced for a feature or bug fix.

### Code consistency

It is also a great opportunity for us to check that whatever new code is being written is consistent with what already exists in the code base.

### Shared responsibility

Approving a code review means that you are the second person who thinks this change is correct and a good idea. Doing this makes you responsible for the code change just as much as the author.

It is the entire DevTools group who owns the code, not just the author. We write code as a group, not as individuals, because on the long-term it's the group that maintains it.

Having a review discussion shares the ownership of the code, because authors and reviewers all put a little bit of themselves in it, and the code that results isn't just the result of one person's work

## What should be avoided in code reviews?

- Style nits that a linter should ideally handle.
- Requests that are outside of the scope of the bug.

## What are some good practices for code reviews?

### Use empathetic language.

More reading:

- [Mindful Communication in Code Reviews](http://amyciavolino.com/assets/MindfulCommunicationInCodeReviews.pdf)
- [How to Do Code Reviews Like a Human](https://mtlynch.io/human-code-reviews-1/)

### Prefer smaller commits over large monolithic commits.

It makes it easier for the reviewer to provide a quality review. It's also easier to get consensus on the change if that change is small. Finally, it's easier to understand the risk, and find regressions.

### Be explicit

Specifically, be explicit about required versus optional changes.

Reviews are conversations, and the reviewee should feel comfortable on discussing and pushing back on changes before making them.

### Be quick

As a reviewer, strive to answer in a couple of days, or at least under a week.

If that is not possible, please update the bug to let the requester know when you'll get to the review. Or forward it to someone else who has more time.

### Ask for help

Know when you're out of your comfort zone as a reviewer, and don't hesitate to ask for an extra review to someone else.

It's fine, you can't know everything, and the more people participate, the more everybody learns, and the more bugs we avoid.

## How do we communicate?

First and foremost, like in any Mozilla-run platforms or events, please abide by [the Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

Maintainers should **lead by example through their tone and language**. We want to build an inclusive and safe environment for working together.

As a reviewer, **double-check your comments**. Just because you're a reviewer and think you have a better solution doesn't mean that's true. Assume **the code author has spent more time thinking about this part of the code than you have** (if you're the reviewer) and might actually be right, even if you originally thought something was wrong. It doesn't take long to look up the code and double-check.

**Being inclusive** is highly important. We shouldn't make any assumptions about the person requesting a review, or about the person you're asking a review from, and always provide as much information as required, in a way that is as inclusive as possible.

The bug will live forever online, and many people will read it long after the author and reviewers are done.

Talking over video/IRC/Slack is a great way to get buy-in and share responsibility over a solution. It is also helpful to resolve disagreement and work through issues in a more relaxed environment.

**Do not criticize** the reviewee, talk about the quality of the code on its own, and not directly how the reviewee wrote the code.

Take the time to thank and point out good code changes.

**Using "please" and “what do you think?”** goes a long way in making others feel like colleagues, and not subordinates.
