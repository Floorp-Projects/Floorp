/*
 * class
 */

class Punny extends Component {
  constructor(props) {
    super();
    this.onClick = this.onClick.bind(this);
  }

  componentDidMount() {}

  onClick() {}

  renderMe() {
    return <div onClick={this.onClick} />;
  }

  render() {}
}

/*
 * CALL EXPRESSIONS - createClass, extend
 */

const TodoView = Backbone.View.extend({
  tagName: "li",

  render: function() {
    console.log("yo");
  }
});

const TodoClass = createClass({
  tagName: "li",

  render: function() {
    console.log("yo");
  }
});

TodoClass = createClass({
  tagName: "li",

  render: function() {
    console.log("yo");
  }
});

app.TodoClass = createClass({
  tagName: "li",

  render: function() {
    console.log("yo");
  }
});

/*
 * PROTOTYPE
 */

function Button() {
  if (!(this instanceof Button)) return new Button();
  this.color = null;
  Nanocomponent.call(this);
}

Button.prototype = Object.create(Nanocomponent.prototype);

var x = function() {};

Button.prototype.createElement = function(color) {
  this.color = color;
  return html`
    <button style="background-color: ${color}">
      Click Me
    </button>
  `;
};

// Implement conditional rendering
Button.prototype.update = function(newColor) {
  return newColor !== this.color;
};
