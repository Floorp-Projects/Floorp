const v2 = Error();
const v3 = [];
const o5 = {
    "scope": "DifferentProcess",
};
const v7 = new ArrayBuffer();
v3.push(v7);
const v10 = serialize(v2, v3, o5);
v10.clonebuffer = v10.arraybuffer.slice(8);
deserialize(v10);
v10["clonebuffer"];
