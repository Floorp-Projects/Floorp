# Storybook for Firefox

[Storybook](https://storybook.js.org/) is a component library to document our
design system, reusable components, and any specific components you might want
to test with dummy data.

## Background

Storybook lists components that can be reused, and helps document
what common elements we have. It can also list implementation specific
components, but they should not be added to the "Design System" section.

Changes to files directly referenced from Storybook (so basically non-chrome://
paths) should automatically reflect changes in the opened browser. If you make a
change to a chrome:// referenced file then you'll need to do a hard refresh
(Cmd+Shift+R/Ctrl+Shift+R) to notice the changes. If you're on Windows you may
need to `./mach build faster` to have the chrome:// URL show the latest version.

## Running Storybook

Installing the npm dependencies and running the `storybook` npm script should be
enough to get Storybook running. This can be done via `./mach storybook`
commands, or with your personal npm/node that happens to be compatible.

### Running with mach commands

This is the recommended approach for installing dependencies and running
Storybook locally.

To install dependencies and start Storybook, just run:

```sh
# This uses npm ci under the hood to install the package-lock.json exactly.
./mach storybook
```

This single command will first install any missing dependencies then start the
local Storybook server. You should run your local build to test in Storybook
since chrome:// URLs are currently being pulled from the running browser, so any
changes to common-shared.css for example will come from your build.

The Storybook server will continue running and will watch for component file
changes. To access your local Storybook preview you can use the `launch`
subcommand:

```sh
# In another terminal:
./mach storybook launch
```

This will run your local browser and point it at `http://localhost:5703`. The
`launch` subcommand will also enable SVG context-properties so the `fill` CSS
property works in storybook.

Alternatively, you can simply navigate to http://localhost:5703/ or run:

```sh
# In another terminal:
./mach run http://localhost:5703/
```

although with these options SVG context-properties won't be enabled, so what's
displayed in Storybook may not exactly reflect how components will look when
used in Firefox.

### Personal npm

You can use your own `npm` to install and run Storybook. Compatibility is up
to you to sort out.

```sh
cd browser/components/storybook
npm ci # Install the package-lock.json exactly so lockfileVersion won't change.
npm run storybook
```

## Updating Storybook dependencies

On occasion you may need to update or add a npm dependency for Storybook.
This can be done using the version of `npm` packaged with `mach`:

```sh
# Install a dev dependency from within the storybook directory.
cd browser/components/storybook && ../../../mach npm i -D your-package
```

## Adding new stories

Storybook is currently configured to search for story files (any file with a
`.stories.(js|mjs)` extension) in `toolkit/content/widgets` and
`browser/components/storybook/stories`.

Stories in `toolkit/content/widgets` are used to document design system
components. The easiest way to use Storybook for non-design system elements is
to add a new `.stories.mjs` file to `browser/components/storybook/stories`.

If you want to colocate your story with the code it is documenting you will need
to add to the `stories` array in the `.storybook/main.js` [configuration
file](https://searchfox.org/mozilla-central/source/browser/components/storybook/.storybook/main.js)
so that Storybook knows where to look for your files.

The Storybook docs site has a [good
overview](https://storybook.js.org/docs/web-components/get-started/whats-a-story)
of what's involved in writing a new story. For convenience you can use the [Lit
library](https://lit.dev/) to define the template code for your story, but this
is not a requirement.
