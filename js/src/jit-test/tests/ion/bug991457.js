function test() {
    this.init();
    for (var i=0; i<10; i++) {
        delete this.blocks[10][9];
        this.collapse_blocks();
    }
    this.look_for_holes();
}
test.prototype.init = function() {
    this.blocks = new Array(20);
    for (var x=0; x<this.blocks.length; x++) {
        this.blocks[x] = new Array(10);
        for (var y=0; y<this.blocks[x].length; y++) {
            this.blocks[x][y] = {};
        }
    }
}
test.prototype.move_block = function(x,y,x1,y1) {
    this.blocks[x][y] = this.blocks[x1][y1];
    if (this.blocks[x][y])
        delete this.blocks[x1][y1];
}
test.prototype.collapse_blocks = function() {
    var didSomething=0;
    do {
        didSomething=0;
        for (var x=0; x<this.blocks.length; x++)
            for (var y=1; y<this.blocks[x].length; y++) {
                if (!this.blocks[x][y] && this.blocks[x][y-1]) {
                    this.move_block(x,y,x,y-1);
                    didSomething=1;
                }
            }
    } while (didSomething);

    do {
        didSomething = 0;
        for (var x=0; x<this.blocks.length-1; x++) {
            if (!this.blocks[x][9] && this.blocks[x+1][9]) {
                for (var y=0; y<this.blocks[x].length; y++)
                    this.move_block(x,y,x+1,y);
                didSomething = 1;
            }
	}
    } while (didSomething);
}
test.prototype.look_for_holes = function() {
    var was_empty = false;
    var n_empty = 0;
    for (var x=0; x<this.blocks.length; x++) {
        var empty = true;
        for (var y=0; y<this.blocks[x].length; y++) {
            if (this.blocks[x][y]) {
                empty = false;
		n_empty++;
	    }
	}
	if (was_empty)
	    assertEq(empty, true);
        was_empty = empty;
    }
    assertEq(n_empty, 190);
}
new test();
