class App extends Component {
  renderHello(name: string, action: ReduxAction, { todos }: Props) {
    return `howdy ${name}`;
  }
}
