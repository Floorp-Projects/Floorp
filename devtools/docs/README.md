# Firefox Developer Tools

Hello! This documentation is for developers who want to work on the developer tools.

If you are looking for general docs about how to use the tools, checkout [this MDN page](https://developer.mozilla.org/en-US/docs/Tools) instead.

If you are looking for a getting started guide on the developer tools, all of this information is documented on the [Hacking](https://wiki.mozilla.org/DevTools/Hacking) wiki page.

[GitBook](https://github.com/GitbookIO/gitbook) is used to generate online documentation from the markdown files here.
Here is how you can re-generate the book:

```bash
# Install GitBook locally
npm install -g gitbook-cli

# Go into the docs directory
cd /path/to/mozilla-central/devtools/docs/

# Generate the docs and start a local server
gitbook serve

# Or just built the book
gitbook build
```