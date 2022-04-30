= Storybook for Firefox

Storybook is a component library to document our design system, reusable
components and any specific components you might want to test with dummy data.

== Background

The storybook will list components that can be reused, and will help document
what common elements we have. It can also list implementation specific
components, but they should not be added to the "Design System" section.

Changes to files directly referenced from the storybook (so basically
non-chrome:// paths) should automatically reflect changes in the opened browser.
If you make a change to a chrome:// referenced file then you'll need to do a
hard refresh (Cmd+Shift+R/Ctrl+Shift+R) to notice the changes.

== Running Storybook

Installing the npm dependencies and running the `storybook` npm script should be
enough to get storybook running. This can be done with your personal npm/node
that happens to be compatible or using `./mach npm`.

=== Running with mach based npm

If you do this a lot, you might want to add an alias like this to your shell's
startup config:

```
alias npm-storybook="./mach npm --prefix=browser/components/storybook"
```

Then running `npm-storybook` from the repo's root directory will work with the
storybook directory.

To start storybook the first time (or if it's been a while since you last
installed):

```
# Install the package-lock.json exactly so lockfileVersion won't change.
# Using the `install` command may affect package-lock.json.
./mach npm --prefix=browser/components/storybook ci
# or
npm-storybook ci
```

Once you've got your dependencies installed you can start storybook. You should
run your local build to test in storybook since chrome:// URLs are currently
being pulled from the running browser, so any changes to common-shared.css for
example will come from your build.

```
# Start the Storybook server.
./mach npm --prefix=browser/components/storybook run start-storybook
# or
npm-storybook run start-storybook
# In another terminal:
./mach run http://localhost:5703/

# On MacOS/Linux, there's a command to start storybook and launch your browser.
./mach npm --prefix=browser/components/storybook run storybook
# or
npm-storybook run storybook
```

=== Personal npm

You can use your own `npm` to install and run storyboook. Compatibility is up
to you to sort out.

```
cd browser/components/storybook
npm ci # Install the package-lock.json exactly so lockfileVersion won't change
npm run storybook
```
