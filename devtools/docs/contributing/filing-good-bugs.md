# Filing good bugs

Getting started working on a bug can be hard, specially if you lack context.

This guide is meant to provide a list of steps to provide the necessary information to resolve a bug. 

* Use a descriptive title. Avoid jargon and abbreviations where possible, they make it hard for other people to find existing bugs, and to understand them.
* Explain the problem in depth and provide the steps to reproduce. Be as specific as possible, and include things such as operating system and version if reporting a bug.
* If you can, list files and lines of code that may need to be modified. Ideally provide a patch for getting started.
* If applicable, provide a test case or document that can be used to test the bug is solved. For example, if the bug title was "HTML inspector fails when inspecting a page with one million of nodes", you would provide an HTML document with one million of nodes, and we could use it to test the implementation, and make sure you're looking at the same thing we're looking at. You could use services like jsfiddle, codepen or jsbin to share your test cases. Other people use GitHub, or their own web server.
* If it's a bug that new contributors can work on, add the keyword `good-first-bug`.
