// Automatically open b2g in a tab
pref("browser.startup.homepage", "chrome://b2g/content/shell.html");

// Disable some painful behavior of fx
pref("startup.homepage_welcome_url", "");
pref("browser.shell.checkDefaultBrowser", "");
pref("browser.sessionstore.max_tabs_undo", 0);
pref("browser.sessionstore.max_windows_undo", 0);
pref("browser.sessionstore.restore_on_demand", false);
pref("browser.sessionstore.resume_from_crash", false);

// Display the devtools on the right of the phone
pref("devtools.toolbox.host", "side");
pref("devtools.toolbox.sidebar.width", 800);

// Disable e10s as we don't want to run shell.html,
// nor the system app OOP, but only inner apps
pref("browser.tabs.remote.autostart", false);
pref("browser.tabs.remote.autostart.1", false);
pref("browser.tabs.remote.autostart.2", false);
