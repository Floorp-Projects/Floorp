var moduleScoped = 1;
let alsoModuleScoped = 2;

function thirdModuleScoped() {}

exports.default = function() {
  console.log("pause here", moduleScoped, alsoModuleScoped, thirdModuleScoped);
};
