/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function getOriginDir(persistence, origin) {
  return getRelativeFile(`storage/${persistence}/${origin}`);
}

function getMetadataFile(persistence, origin) {
  const metadataFile = getOriginDir(persistence, origin);
  metadataFile.append(".metadata-v2");
  return metadataFile;
}

function populateRepository(persistence) {
  const originDir = getOriginDir(persistence, "https+++good-example.com");
  originDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
}

function makeRepositoryUnusable(persistence) {
  // For the purpose of testing, we make a repository unusable by creating an
  // origin directory with the metadata file created as a directory (not a
  // file).
  const metadataFile = getMetadataFile(persistence, "https+++bad-example.com");
  metadataFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
}
