function test() {
  ok(Application, "Check global access to Application");
  
  // I'd test these against a specific value, but that is bound to flucuate
  ok(Application.id, "Check to see if an ID exists for the Application");
  ok(Application.name, "Check to see if a name exists for the Application");
  ok(Application.version, "Check to see if a version exists for the Application");
}
