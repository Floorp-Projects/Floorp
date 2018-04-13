import EmberRouter from '@ember/routing/router';
import config from './config/environment';

const Router = EmberRouter.extend({
  location: config.locationType,
  rootURL: config.rootURL
});

Router.map(function() {
});

window.mapTestFunction = () => {
  window.console.log("pause here", config, Router);
};

export default Router;
