// Files for some of these tests are pre-generated and located in js/src/jit-test/lib/gen.
// There you will also find the script to update these files.
function loadMod(name) {
  return decompressLZ4(os.file.readFile(libdir + "gen/" + name, "binary").buffer)
}
