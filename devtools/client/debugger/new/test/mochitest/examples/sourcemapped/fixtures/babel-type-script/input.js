var moduleScoped = 1;
let alsoModuleScopes = 2;

function thirdModuleScoped() {}

function nonModules() {
  console.log("pause here");
}

Promise.resolve().then(() => {
  // Webpack sets this to undefined initially since this file
  // doesn't export anything, so we overwrite it on the next tick.
  window.babelTypeScript = nonModules;
});
