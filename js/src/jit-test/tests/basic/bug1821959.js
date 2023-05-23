// Transplant an object that is using the shifted-dense-elements optimization
const v1 = this.transplantableObject();
const v2 = v1.object;
Array.prototype.push.call(v2, 0);
Array.prototype.push.call(v2, 0);
Array.prototype.shift.call(v2);
v1.transplant(newGlobal());
