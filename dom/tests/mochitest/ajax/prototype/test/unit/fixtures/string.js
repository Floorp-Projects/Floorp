var attackTarget;
var evalScriptsCounter = 0,
    largeTextEscaped = '&lt;span&gt;test&lt;/span&gt;', 
    largeTextUnescaped = '<span>test</span>';
(2048).times(function(){ 
  largeTextEscaped += ' ABC';
  largeTextUnescaped += ' ABC';
});
