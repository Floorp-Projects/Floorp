function addConstraint(blaat) {
    return blaat.v1
}

function ScaleConstraint() {
    this.direction = null
    this.v1 = {};
    addConstraint(this);
}

function EqualityConstraint() {
    this.v1 = {};
    addConstraint(this);
}

function deltaBlue() {
    new EqualityConstraint();
    new ScaleConstraint();
}

for (var n = 0; n<100; n++) {
    deltaBlue()
}
