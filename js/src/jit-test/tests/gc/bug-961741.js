function r() {
    for (var x in undefined) {}
}
setObjectMetadataCallback(true);
r();
