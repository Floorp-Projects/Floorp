# How to fix a bug

## Make sure you understand what needs to be done

If you're not quite sure of this, please add a comment requesting more information in the bug itself. It is absolutely fine to also talk to someone via other means (e.g. irc, email, in the office kitchen...), but it is good to come back to the bug and add the extra information, just in case you stop working on the bug at some point and someone else has to pick work where you left it.

## Find out where are the files that need changing

In an ideal world, the bug has this information from the start, but very often it doesn't.

If you're not yet familiar with the codebase, the [files and directories overview](../files/README.md) might help you find where things are.

If you know what you're looking for (e.g. a string that has a typo and needs correcting, or a function name that needs modifying), you can use a source code search engine:

* [Searchfox](http://searchfox.org/mozilla-central/source)

It is a good idea to [add smart keyword searches](https://support.mozilla.org/en-US/kb/how-search-from-address-bar) for DXR to search faster.

You can also use your operating system's command line. For example, let's search for occurrences of `TODO` in the code base.

Within your command line prompt, `cd` to the `devtools` directory:

```bash
cd ~/mozilla-central/devtools # use your actual folder name!
grep -r 'TODO' .
```

This will list all the places in the DevTools code that contain the `TODO` string. If there are too many instances, you can combine the above with `less`, to scroll and paginate the output of `grep`:

```bash
grep -r 'TODO' . | less
```

Press `q` to exit.

If after all of this you still can't find your bearings, add a comment in the bug asking for more information.

## How do you know that you have found what you were looking for?

There are a few options to confirm that you found the right files:

### If this is about changing a string...

Edit the file(s), and change the string (e.g. fix a typo). Rebuild, and run again:

```bash
./mach build
./mach run
```
Then go to the panel that displays the string you wanted to change.

Does the typo still occur? Or is the string being displayed the correct one now?

### If this is about changing JavaScript code...

If you think you found the right file to edit, add some logging statement on a place of the code which is likely to be executed (for example, on a class constructor):

```javascript
// For front-end code
console.log('hello friends\n');

// Sometimes it's hard to spot your output. Emojis can help here really well.
console.log('👗👗👗', 'This is your logged output!');
```

Or...

```javascript
// For server code
dump('hello friends\n');
```

TIP: Whether to use one or another depends on the type of bug you're working on, but if you've just started in DevTools, it's highly likely you'll take a front-end bug first.

Then rebuild and run again:

```bash
./mach build
./mach run
```

Go to the panel or initiate the action that is likely to result on the code being executed, and pay close attention to the output in your console.

Can you see `hello friends`? Then you found the file that you were looking for.

It's possible that you'll get a lot of other messages you don't care about now, but we can use `grep` to filter:

```bash
./mach run | grep hello friends
```

This will only show lines that contain `hello friends`. If you get an empty output after trying to get the code to execute, maybe this isn't the right file, or maybe you didn't trigger the action.

And that means it's time to ask for more information in the bug or from your colleagues. Tell them what you tried, so they don't have to figure that out themselves (saves everyone some time!).

### If this is about changing CSS code...

If you think you have found the right file and the right CSS selector, you could try to edit the file to insert some outrageously colourful rule (for example, a really thick bright blue border).

```css
border: 4px solid blue;
```

Check if the changes show up by rebuilding your local changes.

```bash
./mach build faster
./mach run
```

## NEXT: do whatever needs doing

This will always depend on the specific bug you're working on, so it's hard to provide guidance here.

The key aspect here is that if you have questions, you should not hesitate to ask. Ask your mentor, your manager, or [get in touch](https://firefox-dev.tools/#getting-in-touch). **You should just not get stuck**.

Some people find it difficult to recognise or even admit they're in such a situation, so some ways to describe 'stuck' could be:

* you've tried everything you could think of, nothing works, and do not know what else to do.
* you have various ideas for things that can be done, and do not know which one to go for.
* you have not learned anything new about the problem in the last one or two days.
* you're starting to think about abandoning this bug and doing something else instead.
* you don't know what to do, but are afraid of asking for help.

If you think *any* of the above describes you, ask for help!

Another tip you can use if you're afraid that you're wasting people's time is to timebox. For example, give yourself 2 hours to investigate. If you can't figure anything after that time has elapsed, stop and ask for help. It might be that you needed a key piece of information that was missing in the bug description, or you misunderstood something, or maybe even that you found a bug and that's why things didn't work even if they should! This is why it's important to call for help sooner rather than later.

### Useful references

#### Coding standards

If it's your first time contributing, the documentation on [coding standards](./coding-standards.md) might have answers to questions such as what style of code to use, how to name new files (if you have to add any), tools we use to run automated checks, etc.

#### Specialised guides

We also have a few guides explaining how to work on specific types of problems, for example: [investigating performance issues](./performance.md), or [writing efficient React code](./react-performance-tips.md). Please have a look at the sidebar or use the search box on top of the sidebar to see if there's something written about the type of bug you're working on.

If not, maybe you'll be able to contribute with one by the time you fix your bug!

#### MDN

[MDN Web Docs](http://developer.mozilla.org/) (also known as *MDN*) has a lot of information about HTML, CSS, JS, DOM, Web APIs, Gecko-specific APIs, and more.

## Run tests

We have several types of automated tests to help us when developing.

Some, like the linting tests, address coding style; others address functionality, such as unit and integration tests. This page has more [details on types of tests and how to run them](../tests/writing-tests.md).

You might want to run the unit and integration types of tests quite frequently, to confirm you're not breaking anything else. Depending on what you're doing, it might be even possible to run just one test file which addresses the specific change you're implementing:

```bash
./mach test devtools/path/to/test.js
```

Sometimes you might want to run a number of tests which are related to the bug you're fixing:

```bash
./mach test devtools/path/to/test-thing-*.js
```

At the beginning, it is entirely possible that you have no idea of where the tests are for the thing you're working on. Please ask for help! You will eventually learn your way around.

It is good etiquette to ensure the tests pass locally before asking for a code review. This includes linting tests. To run them, please [configure your system to run ESlint](./eslint.md), and then you can execute:

```bash
./mach eslint devtools/path/to/files/you/changed
```

Our tool for code review will run the linter automatically as well, but if you run this locally you'll get instant feedback, and avoid having to send an updated commit again.

## Time for a review

When you think you have fixed the bug, first let's celebrate a bit! Yay! Well done 😀

And now it's time to [get your code reviewed](./code-reviews.md).
