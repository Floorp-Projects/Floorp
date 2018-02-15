var moduleScoped = 1;
let alsoModuleScoped = 2;

function thirdModuleScoped() {}

export default function() {
  console.log("pause here", moduleScoped, alsoModuleScoped, thirdModuleScoped);
}
