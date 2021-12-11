<template>
  <div class="hello">
    <h1>{{ msg }}</h1>
  </div>
</template>

<script>
var moduleVar = "data";

export default {
  name: 'HelloWorld',
  data () {
    var fnVar = 4;

    return {
      msg: 'Welcome to Your Vue.js App'
    };
  }
};
</script>

<style scoped>
a {
  color: red;
}
</style>
