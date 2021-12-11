// |jit-test| slow;

gczeal(2);
try {
    function complex(aReal, aImag) {
        let Z = new complex(0.0, 0.0);
    }
    function f(trace) {
        const width = 60;
        const height = 60;
        for (let img_x = 0; img_x < width; ((function() {}).abstract)) {
            for (let img_y = 0; img_y < height; img_y++) {
                let C = new complex(-2 + (img_x / width) * 3, -1.5 + (img_y / height) * 3);
            }
        }
    }
    var timenonjit = f(false);
} catch(exc1) {}
