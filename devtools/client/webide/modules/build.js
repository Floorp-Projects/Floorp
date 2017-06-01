/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu, Cc, Ci} = require("chrome");

const { Task } = require("devtools/shared/task");
const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});
const Subprocess = require("sdk/system/child_process/subprocess");

const ProjectBuilding = exports.ProjectBuilding = {
  fetchPackageManifest: Task.async(function* (project) {
    let manifestPath = OS.Path.join(project.location, "package.json");
    let exists = yield OS.File.exists(manifestPath);
    if (!exists) {
      // No explicit manifest, try to generate one if possible
      return this.generatePackageManifest(project);
    }

    let data = yield OS.File.read(manifestPath);
    data = new TextDecoder().decode(data);
    let manifest;
    try {
      manifest = JSON.parse(data);
    } catch (e) {
      throw new Error("Error while reading WebIDE manifest at: '" + manifestPath +
                      "', invalid JSON: " + e.message);
    }
    return manifest;
  }),

  /**
   * For common frameworks in the community, attempt to detect the build
   * settings if none are defined.  This makes it much easier to get started
   * with WebIDE.  Later on, perhaps an add-on could define such things for
   * different frameworks.
   */
  generatePackageManifest: Task.async(function* (project) {
    // Cordova
    let cordovaConfigPath = OS.Path.join(project.location, "config.xml");
    let exists = yield OS.File.exists(cordovaConfigPath);
    if (!exists) {
      return;
    }
    let data = yield OS.File.read(cordovaConfigPath);
    data = new TextDecoder().decode(data);
    if (data.contains("cordova.apache.org")) {
      return {
        "webide": {
          "prepackage": "cordova prepare",
          "packageDir": "./platforms/firefoxos/www"
        }
      };
    }
  }),

  hasPrepackage: Task.async(function* (project) {
    let manifest = yield ProjectBuilding.fetchPackageManifest(project);
    return manifest && manifest.webide && "prepackage" in manifest.webide;
  }),

  // If the app depends on some build step, run it before pushing the app
  build: Task.async(function* ({ project, logger }) {
    if (!(yield this.hasPrepackage(project))) {
      return;
    }

    let manifest = yield ProjectBuilding.fetchPackageManifest(project);

    logger("start");
    try {
      yield this._build(project, manifest, logger);
      logger("succeed");
    } catch (e) {
      logger("failed", e);
    }
  }),

  _build: Task.async(function* (project, manifest, logger) {
    // Look for `webide` property
    manifest = manifest.webide;

    let command, cwd, args = [], env = [];

    // Copy frequently used env vars
    let envService = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
    ["HOME", "PATH"].forEach(key => {
      let value = envService.get(key);
      if (value) {
        env.push(key + "=" + value);
      }
    });

    if (typeof (manifest.prepackage) === "string") {
      command = manifest.prepackage.replace(/%project%/g, project.location);
    } else if (manifest.prepackage.command) {
      command = manifest.prepackage.command;

      args = manifest.prepackage.args || [];
      args = args.map(a => a.replace(/%project%/g, project.location));

      env = env.concat(manifest.prepackage.env || []);
      env = env.map(a => a.replace(/%project%/g, project.location));

      if (manifest.prepackage.cwd) {
        // Normalize path for Windows support (converts / to \)
        let path = OS.Path.normalize(manifest.prepackage.cwd);
        // Note that Path.join also support absolute path and argument.
        // So that if cwd is absolute, it will return cwd.
        let rel = OS.Path.join(project.location, path);
        let exists = yield OS.File.exists(rel);
        if (exists) {
          cwd = rel;
        }
      }
    } else {
      throw new Error("pre-package manifest is invalid, missing or invalid " +
                      "`prepackage` attribute");
    }

    if (!cwd) {
      cwd = project.location;
    }

    logger("Running pre-package hook '" + command + "' " +
           args.join(" ") +
           " with ENV=[" + env.join(", ") + "]" +
           " at " + cwd);

    // Run the command through a shell command in order to support non absolute
    // paths.
    // On Windows `ComSpec` env variable is going to refer to cmd.exe,
    // Otherwise, on Linux and Mac, SHELL env variable should refer to
    // the user chosen shell program.
    // (We do not check for OS, as on windows, with cygwin, ComSpec isn't set)
    let shell = envService.get("ComSpec") || envService.get("SHELL");
    args.unshift(command);

    // For cmd.exe, we have to pass the `/C` option,
    // but for unix shells we need -c.
    // That to interpret next argument as a shell command.
    if (envService.exists("ComSpec")) {
      args.unshift("/C");
    } else {
      args.unshift("-c");
    }

    // Subprocess changes CWD, we have to save and restore it.
    let originalCwd = yield OS.File.getCurrentDirectory();
    try {
      yield new Promise((resolve, reject) => {
        Subprocess.call({
          command: shell,
          arguments: args,
          environment: env,
          workdir: cwd,

          stdout: data =>
            logger(data),
          stderr: data =>
            logger(data),

          done: result => {
            logger("Terminated with error code: " + result.exitCode);
            if (result.exitCode == 0) {
              resolve();
            } else {
              reject("pre-package command failed with error code " + result.exitCode);
            }
          }
        });
      }).then(() => {
        OS.File.setCurrentDirectory(originalCwd);
      });
    } catch (e) {
      throw new Error("Unable to run pre-package command '" + command + "' " +
                      args.join(" ") + ":\n" + (e.message || e));
    }
  }),

  getPackageDir: Task.async(function* (project) {
    let manifest = yield ProjectBuilding.fetchPackageManifest(project);
    if (!manifest || !manifest.webide || !manifest.webide.packageDir) {
      return project.location;
    }
    manifest = manifest.webide;

    let packageDir = OS.Path.join(project.location, manifest.packageDir);
    // On Windows, replace / by \\
    packageDir = OS.Path.normalize(packageDir);
    let exists = yield OS.File.exists(packageDir);
    if (exists) {
      return packageDir;
    }
    throw new Error("Unable to resolve application package directory: '" + manifest.packageDir + "'");
  })
};
