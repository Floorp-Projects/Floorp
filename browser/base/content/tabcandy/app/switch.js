(function(){

// ##########
window.Switch = {
  // ----------
  get name() {
    var chunks = window.location.toString().split('/');
    var myName = chunks[chunks.length - 2];
    return myName;
  },
      
  // ----------
  insert: function(selector, style) {
    var myName = this.name;
    var html = '<div style="font-size: 11px; margin: 5px 0px 0px 5px;'
      + style
      + '">Candy: <select id="visualization-select">';
      
    Utils.getVisualizationNames(function(names) {
      var count = names.length;
      var a;
      for(a = 0; a < count; a++) {
        var name = names[a];
        html += '<option value="'
          + name
          + '"'
          + (name == myName ? ' selected="true"' : '')
          + '>'
          + name
          + '</option>';
      }
      
      html += '<option disabled="disabled">----------</option>';
      html += '<option value="">Home</option>';

      html += '</select>';
      $(selector).prepend(html);
      $('#visualization-select').change(function () {
        var name = $(this).val();
        if(name)
          location.href = '../' + name + '/index.html';
        else
          location.href = '../../index.html';
      });
    });
  }
};

})();
