var moduleScoped = 1;
let alsoModuleScopes = 2;

function thirdModuleScoped() {}

function nonModules() {
  console.log("pause here");
}

exports.default = nonModules;
