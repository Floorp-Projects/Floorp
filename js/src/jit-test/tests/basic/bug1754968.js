const root = {a:0, b:0};
const subclass = Object.create(root);
const instance = Object.create(subclass);
Object.assign(subclass, root);
