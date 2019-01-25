// lifted from https://gist.github.com/kitze/23d82bb9eb0baabfd03a6a720b1d637f
export const ConditionalWrapper = ({condition, wrap, children}) => (condition ? wrap(children) : children);
