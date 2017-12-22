let arrow;

class foo extends class { } {
    constructor() {
        arrow = () => this;
        super();
    }
}

assertEq(new foo(), arrow());

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
