# Pocket Integration

This is mostly everything related to Pocket in the browser. There are a few exceptions, like newtab, some reader mode integration, and some JSWindowActors, that live in other places.

Primarily though this directory includes code for setting up the save to Pocket button, setting up the panels that are used once the Pocket button is clicked, setting up the Pocket context menu item, and a little scaffolding for the reader mode Pocket integration.

## Basic Code Structure

We have three primary areas code wise.

There are some JSMs that handle communication with the browser. This includes some telemetry, some API functions usable by other parts of the browser, like newtab, and some initialization and setup code. These files live in `/content`

There is also some standard js, html, and css that run inside the panels. Panels are the contents inside the drop downs if you click the save to Pocket button. Panels run in their own browser process, and have their own js. This js also has a build/bundle step. These files live in `/content/panels`. We have three panels. There is a sign up panel that is displayed if you click the save to Pocket button while not signed in. There is a saved panel, if you click the save to Pocket button while signed in, and on a page that is savable. Finally there is a home panel, if you click the save to Pocket button while signed in, on a page that is not savable, like about:home.

## Build Panels

We use webpack and node to build the panel bundle. So if you change anything in `/content/panels/js` or `/content/panels/css`, you probably need to build the bundle.

The build step makes changes to the bundle files, that need to be included in your patch.

### Prerequisites

You need node.js installed, and a working local build of Firefox. The current or active version of node is probably fine. At the time of this writing, node version 14 and up is active, and is recommended.

### How to Build

From `/browser/components/pocket`

If you're making a patch that's ready for review:
run `npm install`
then `npm run build`

For active development instead of `npm run build` use `npm run watch`, which should update bundles as you work.

## React and JSX

We use React and JSX for most of the panel html and js. You can find the React components in `/content/panels/js/components`.

We are trying to keep the React implementation and dependencies as small as possible.
