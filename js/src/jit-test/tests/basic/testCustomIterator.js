function my_iterator_next() {
    if (this.i == 10) {
        this.i = 0;
        throw this.StopIteration;
    }
    return this.i++;
}
function testCustomIterator() {
    var o = {
        __iterator__: function () {
            return {
                i: 0,
                next: my_iterator_next,
                StopIteration: StopIteration
            };
        }
    };
    var a=[];
    for (var k = 0; k < 100; k += 10) {
        for(var j in o) {
            a[k + (j >> 0)] = j*k;
        }
    }
    return a.join();
}
assertEq(testCustomIterator(), "0,0,0,0,0,0,0,0,0,0,0,10,20,30,40,50,60,70,80,90,0,20,40,60,80,100,120,140,160,180,0,30,60,90,120,150,180,210,240,270,0,40,80,120,160,200,240,280,320,360,0,50,100,150,200,250,300,350,400,450,0,60,120,180,240,300,360,420,480,540,0,70,140,210,280,350,420,490,560,630,0,80,160,240,320,400,480,560,640,720,0,90,180,270,360,450,540,630,720,810");
