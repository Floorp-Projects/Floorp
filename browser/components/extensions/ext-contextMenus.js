extensions.registerPrivilegedAPI("contextMenus", (extension, context) => {
  return {
    contextMenus: {
      create() {},
      removeAll() {},
    },
  };
});
