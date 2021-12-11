## Actions

### Best Practices

#### Scheduling Async Actions

There are several use-cases with async actions that involve scheduling:

* we do one action and cancel subsequent actions
* we do one action and subsequent calls wait on the initial call
* we start an action and show a loading state

If you want to wait on subsequent calls you need to store action promises.
[ex][req]

If you just want to cancel subsequent calls, you can keep track of a pending
state in the store. [ex][state]

The advantage of adding the pending state to the store is that we can use that
in the UI:

* disable/hide the pretty print button
* show a progress ui

[req]: https://github.com/firefox-devtools/debugger/blob/master/src/actions/sources/loadSourceText.js
[state]: https://github.com/firefox-devtools/debugger/blob/master/src/reducers/sources.js
