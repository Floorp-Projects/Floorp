// ##########
window.Items = {
  // ----------  
  getTopLevelItems: function() {
    var items = [];
    
    $('.tab').each(function() {
      $this = $(this);
      if(!$this.data('group'))
        items.push($this.data('tabItem'));
    });
    
    $('.group').each(function() {
      items.push($(this).data('group'));
    });
    
    return items;
  }
};

