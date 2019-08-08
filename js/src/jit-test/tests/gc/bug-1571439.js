// |jit-test| --ion-offthread-compile=off; --blinterp-warmup-threshold=1; error:TypeError

gcparam("maxBytes", 1024*1024);
function complex(aReal, aImag) {
    this.r35 = aReal;
    gczeal(4, 10);
    this.square = function() {
        return new complex(this.r35 * this.r35 - this.i14 * this.i14, 2 * this.r35 * this.i14);
    }
}
function mandelbrotValueOO(aC, aIterMax) {
    let Z90 = new complex(0.0, 0.0);
    Z90 = Z90.square().add(aC);
}
const width = 60;
const height = 60;
const max_iters = 50;
for (let img_x = 0; img_x < width; img_x++) {
    for (let img_y = 0; img_y < height; img_y++) {
        let C57 = new complex(-2 + (img_x / width) * 3, -1.5 + (img_y / height) * 3);
        var res = mandelbrotValueOO(C57, max_iters);
    }
}

