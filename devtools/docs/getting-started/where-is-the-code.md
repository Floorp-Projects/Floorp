# Where is the code? (or: `mozilla-central` vs `devtools-html`)

Most of the code is hosted in the Firefox repository (we call it `mozilla-central`, often abbreviated as `m-c`), in the [devtools](https://dxr.mozilla.org/mozilla-central/source/devtools) folder. Development of newer pieces of the tools is happening in GitHub, on the [devtools-html](https://github.com/devtools-html/) organisation.

<!--TODO: table listing components and locations (m-c vs github)-->

Code in `m-c` takes longer to obtain and build, as it involves checking out Firefox's repository and installing tools such as a compiler to build a version of Firefox in your machine.

On the other hand, the repositories in `devtools-html` are more straightforward if you're used to *the GitHub workflow*: you clone them, and then run `npm install && npm run` or similar. Roughly, you can work with each repository individually, and we periodically generate JavaScript bundles that are then copied into `m-c`.

Even if you only want to work on a tool whose code is on `devtools-html`, you might still need to go through the step of getting and compiling the code from `mozilla-central` in order to do integration work (such as updating a tool bundle).

From now on, this guide will focus on building the full DevTools within Firefox. Please refer to individual project instructions for tools hosted in `devtools-html`.

