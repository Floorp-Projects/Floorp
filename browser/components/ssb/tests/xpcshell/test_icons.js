/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function icons() {
  let ssb = await SiteSpecificBrowser.createFromManifest(
    parseManifest("https://www.mozilla.org/", {
      icons: [
        {
          src: "data:b",
          sizes: "24x24",
        },
        {
          src: "data:a",
          sizes: "16x16 32x32",
        },
        {
          src: "data:c",
          sizes: "any",
        },
        {
          src: "data:d",
          sizes: "128x128",
        },
      ],
    })
  );

  Assert.equal(ssb.getIcon(1).src, "data:a");
  Assert.equal(ssb.getIcon(15).src, "data:a");
  Assert.equal(ssb.getIcon(16).src, "data:a");
  Assert.equal(ssb.getIcon(17).src, "data:b");
  Assert.equal(ssb.getIcon(23).src, "data:b");
  Assert.equal(ssb.getIcon(24).src, "data:b");
  Assert.equal(ssb.getIcon(25).src, "data:a");
  Assert.equal(ssb.getIcon(31).src, "data:a");
  Assert.equal(ssb.getIcon(32).src, "data:a");
  Assert.equal(ssb.getIcon(33).src, "data:d");
  Assert.equal(ssb.getIcon(127).src, "data:d");
  Assert.equal(ssb.getIcon(128).src, "data:d");
  Assert.equal(ssb.getIcon(129).src, "data:c");
  Assert.equal(ssb.getIcon(5000).src, "data:c");
});
