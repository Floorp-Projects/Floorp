# React Tips

Learn various tips and tricks for working with our React code.

## Hot Reloading

If you followed [this
rule](react-guidelines.md#export-the-component-directly-and-create-factory-on-import)
about exporting components, and are using the BrowserLoader, you can
hot reload any React component. Just turn on the pref
`devtools.loader.hotreload`, re-open the devtools, and you should be
able to save any React component and instantly see changes.

This does not reset the state of your app, so you can quickly se
changes in the same context.

## Profiling

We need information here about how to use React.addons.Perf to profile the app.