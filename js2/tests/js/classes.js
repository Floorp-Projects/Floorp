class A {
    static var x = 0;

    static function f()
    {
        return A.x++;
    }
}    

class Point3D {
    var x = 0, y = 0, z = 0;

    function set(x, y, z) {
        this.x = x;
        this.y = y;
        this.z = z;
        return this;
    }
    
    function setX(x) { this.x = x; }
    function getX() { return this.x; }
    
    function setY(y) { this.y = y; }
    function getY() { return this.y; }
    
    function setZ(z) { this.z = z; }
    function getZ() { return this.z; }
}
