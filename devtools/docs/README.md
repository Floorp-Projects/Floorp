# Firefox Developer Tools

**Hello!**

This documentation is for developers who want to work on the developer tools. [Get started here](./getting-started/).

If you are looking for end user documentation, check out [this MDN page](https://developer.mozilla.org/en-US/docs/Tools) instead.

Happy developing!

## About this documentation

This guide is built with MarkDown files and [GitBook](https://github.com/GitbookIO/gitbook).

The source code for this documentation is distributed with the source code for the tools, in the `docs/` folder.

If you want to contribute to the documentation, [clone the repository](./getting-started/build.md#getting-the-code), make your changes locally, and then regenerate the book to see how it looks like before submitting a patch:

```bash
# Install GitBook locally
npm install -g gitbook-cli

# Go into the docs directory
cd /path/to/mozilla-central/devtools/docs/

# Generate the docs and start a local server
gitbook serve

# You can now navigate to localhost:4000 to see the output

# Or build the book only (this places the output into `docs/_book`)
gitbook build
```
