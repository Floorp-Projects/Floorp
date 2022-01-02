
for (x in [0]) {
    (function() {
        return Object.propertyIsEnumerable
    })().call([0], x)
}
