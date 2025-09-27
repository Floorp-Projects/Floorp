import type { Manifest } from "../type.ts";

const { ImageTools } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/ImageTools.sys.mjs",
);

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);

export class LinuxSupport {
  private static nsIFile = Components.Constructor(
    "@mozilla.org/file/local;1",
    Ci.nsIFile,
    "initWithPath",
  );

  private static iconDir: string = "~/.local/share/icons/";

  private static iconFile = (ssb: Manifest) => {
    return new LinuxSupport.nsIFile(
      PathUtils.join(this.iconDir, `floorp-${ssb.name.toLowerCase()}.png`),
    );
  }

  private static applicationDir: string = "~/.local/share/applications/";

  private static desktopFile = (ssb: Manifest) => {
    return new LinuxSupport.nsIFile(PathUtils.join(
      this.applicationDir,
      `floorp-${ssb.name.toLowerCase()}.desktop`,
    ));
  }

  async install(ssb: Manifest) {
    const icon = ssb.icon;
    let iconFile = LinuxSupport.iconFile(ssb);
    if (icon) {
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(icon),
      );
      ImageTools.saveIcon(container, 128, 128, iconFile);
    } else {
      iconFile = null;
    }

    let command = Services.dirsvc.get("XREExeF",Ci.nsIFile).path;

    if (FileUtils.File("/.flatpak-info").exists()) {
      command = "flatpak run one.ablaze.floorp";
    }

    const desktopFile = LinuxSupport.desktopFile(ssb);
    await IOUtils.write(
      desktopFile.path,
      new TextEncoder().encode(
        `[Desktop Entry]
Type=Application
Terminal=false
Name=${ssb.name}
Exec=${command} --profile "${PathUtils.profileDir}" --start-ssb "${ssb.id}"
Icon=${iconFile?.path}`
    )
  );
  }

  async uninstall(ssb: Manifest) {
    const desktopFile = LinuxSupport.desktopFile(ssb);
    try {
      await IOUtils.remove(desktopFile.path);
    } catch (e) {
      console.error(e);
    }

    const iconFile = LinuxSupport.iconFile(ssb);
    try {
      await IOUtils.remove(iconFile.path, {
        recursive: true,
        ignoreAbsent: true,
      });
    } catch (e) {
      console.error(e);
    }
  }
}