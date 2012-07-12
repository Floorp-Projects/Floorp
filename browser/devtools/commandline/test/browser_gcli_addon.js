function test() {
  DeveloperToolbarTest.test("about:blank", function GAT_test() {
    function GAT_ready() {
      Services.obs.removeObserver(GAT_ready, "gcli_addon_commands_ready", false);

      DeveloperToolbarTest.checkInputStatus({
        typed: "addon list dictionary",
        status: "VALID"
      });
      DeveloperToolbarTest.checkInputStatus({
        typed: "addon list extension",
        status: "VALID"
      });
      DeveloperToolbarTest.checkInputStatus({
        typed: "addon list locale",
        status: "VALID"
      });
      DeveloperToolbarTest.checkInputStatus({
        typed: "addon list plugin",
        status: "VALID"
      });
      DeveloperToolbarTest.checkInputStatus({
        typed: "addon list theme",
        status: "VALID"
      });
      DeveloperToolbarTest.checkInputStatus({
        typed: "addon list all",
        status: "VALID"
      });
      DeveloperToolbarTest.checkInputStatus({
        typed: "addon disable Test_Plug-in_1.0.0.0",
        status: "VALID"
      });
      DeveloperToolbarTest.checkInputStatus({
        typed: "addon enable Test_Plug-in_1.0.0.0",
        status: "VALID"
      });
      DeveloperToolbarTest.exec({ completed: false });
      finish();
    }
    Services.obs.addObserver(GAT_ready, "gcli_addon_commands_ready", false);
  });
}
