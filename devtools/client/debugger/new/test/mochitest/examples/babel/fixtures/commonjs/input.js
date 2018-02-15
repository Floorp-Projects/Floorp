var moduleScoped = 1;
let alsoModuleScoped = 2;

function thirdModuleScoped() {}

module.exports = function() {
  console.log("pause here", moduleScoped, alsoModuleScoped, thirdModuleScoped);
};
